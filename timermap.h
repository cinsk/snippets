/* -*-c++-*- */

#ifndef TIMERMAP_H__
#include <map>
#include <stdexcept>
#include <pthread.h>
#include <time.h>

#include "xerror.h"

//
// TimerMap<K, V> provides rudimenrary associative array that provides
// only get()/set() interface.
//
// TimerMap CTOR accept two arguments, TimerMap(EXPIRATION, REFRESH).
// EXPIRATION is the expiration time in second, so that a key will
// only alive for EXPIRATION second(s).  The optional REFRESH specify
// whether a retrival operation refresh the EXPIRATION or not.
//
// Suppose we have:
//
//   typedef TimerMap<int, boost::shared_ptr<Foo> > TMAP;
//   TMAP m;
//
// To retrive a value (e.g. where key is 0), you need a lock:
//
// 1)
//   try {
//     boost::shared_ptr<Foo> ptr = m.get(0);
//     ptr->SOME_OPERATION();
//   }
//   catch (std::out_of_range &e) {
//     ...
//   }
//
// Note that the lock is not released as long as the return value
// of m.getptr() is alive.  Thus, you should make the try block
// very narrow.  Thus, until 'ptr' is out-of-scope, you can
// safely dereference 'ptr', and it will never expired as long as
// you're in the try block.
//

// TODO: currently, there is only one lock, so that only one thread
//       can access the element.  I need to add additional lock per
//       element, so that several threads can access elements, as long
//       as each thread access different element.

#define XDEBUG(...)  ((void)0)

template <typename K, typename V>
class TimerMap {

  struct TMENT {
    time_t tm;
    ::pthread_mutex_t lck;

    TMENT() : tm(time(0)) {
      ::pthread_mutexattr_t attr;
      ::pthread_mutexattr_init(&attr);
      ::pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
      ::pthread_mutex_init(&lck, &attr);
      ::pthread_mutexattr_destroy(&attr);
    }

    TMENT(const TMENT &ent) : tm(ent.tm) {
      ::pthread_mutexattr_t attr;
      ::pthread_mutexattr_init(&attr);
      ::pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
      ::pthread_mutex_init(&lck, &attr);
      ::pthread_mutexattr_destroy(&attr);
    }

    ~TMENT() {
      int ret;
      if ((ret = ::pthread_mutex_destroy(&lck)) != 0) {
        XDEBUG(ret, "locked mutex at pthread_mutex_destroy()");
        ::abort();
      }
    }

    void lock(const char *prefix = 0)    {
      XDEBUG(0, "%s TMENT: lock", prefix ? prefix : 0);
      pthread_mutex_lock(&lck);
    }
    void unlock(const char *prefix = 0)  {
      XDEBUG(0, "%s TMENT: unlock", prefix ? prefix : 0);
      pthread_mutex_unlock(&lck);
    }
    bool trylock(const char *prefix = 0) {
      bool ret = (pthread_mutex_trylock(&lck) == 0);
      XDEBUG(0, "%s TMENT trylock %s",
             prefix ? prefix : "", ret ? "locked" : "nolock");
      return ret;
    }

    void refresh() { tm = time(0); }
    bool expired(time_t now, time_t expiration) {
      return (now - tm) > expiration;
    }
  };

  typedef std::map<K, TMENT> tmap_type;
  typedef std::map<K, V> map_type;

  tmap_type tmap_;

  // Note to maintainers:
  //   Access to map_[k] MUST be synchronized with tmap_[k].
  //   Specifically, to access map_[k], you need to tmap_[k].lock()
  map_type map_;

  TimerMap(const TimerMap &);

  pthread_t timer_thread;
  pthread_mutex_t lock_;

  int expiration_;
  bool refresh_;

  void lock(const char *prefix = 0)    {
    XDEBUG(0, "%s lock", prefix ? prefix : "");
    pthread_mutex_lock(&lock_);
  }
  void unlock(const char *prefix = 0)  {
    XDEBUG(0, "%s unlock", prefix ? prefix : "");
    pthread_mutex_unlock(&lock_);
  }
  bool trylock(const char *prefix = 0) {
    bool ret = (pthread_mutex_trylock(&lock_) == 0);
    XDEBUG(0, "%s trylock %s",
           prefix ? prefix : "", ret ? "locked" : "nolock");
    return ret;
  }

  // This function will release the lock holding the element pointed by I,
  // and remove the element from TimerMap if it is expired.
  bool remove_if_expired(typename tmap_type::iterator i) {
    // assuming that 'this' is locked.
    // assuming that (*i).second (type: TMENT) is locked.

    if (i == tmap_.end())
      return false;
    TMENT &ent = (*i).second;
    time_t now = ::time(0);

    if (ent.expired(now, expiration_)) {
      typename map_type::iterator j = map_.find((*i).first);
      map_.erase(j);
      ent.unlock("remove");
      tmap_.erase(i);
      return true;
    }
    ent.unlock("remove");
    return false;
  }

  // This function will scan all elements and remove the elements that
  // are expired.
  void remove_expired() {
    // should be called with tmap->lock_ is locked.
    typename tmap_type::iterator i = tmap_.begin();

    while (i != tmap_.end()) {
      TMENT &ent = (*i).second;

      if (ent.trylock("remove")) {
        // To speed-up, remove expired items iff we can get the lock
        // instantly.
        remove_if_expired(i++);
      }
    }
  }

public:
  static void *timer_main(void *arg);
  static const int timer_interval = 1;

  typedef typename std::map<K, V>::key_type key_type;
  typedef typename std::map<K, V>::mapped_type mapped_type;
  typedef typename std::map<K, V>::value_type value_type;
  typedef V element_type;

  explicit TimerMap(int expiration, bool refresh = false)
    : map_(), expiration_(expiration), refresh_(refresh) {
    // TODO: change to recusive lock
    int ret;
    ::pthread_mutexattr_t attr;
    ::pthread_mutexattr_init(&attr);
    ::pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    ::pthread_mutex_init(&lock_, &attr);
    ::pthread_mutexattr_destroy(&attr);

    ret = pthread_create(&timer_thread, 0, timer_main, (void *)this);
    if (ret != 0)
      xerror(0, ret, "pthread_create() failed");
  }

  ~TimerMap() {
    // TODO: lock_ must be unlocked!
    void *pret;

    ::pthread_cancel(timer_thread);
    ::pthread_join(timer_thread, &pret);
    // XDEBUG(0, "timer_thread returns %p", pret);
    ::pthread_mutex_destroy(&lock_);
  }

  //key_type foo(const key_type &k) { return k; }

  void set(const key_type &k, const V &v) {
    lock("set");
    typename tmap_type::iterator i = tmap_.find(k);
    if (i == tmap_.end()) {     // new entry
      TMENT ent = TMENT();
      tmap_[k] = ent;
      ent.lock("set");
      unlock("set");
      map_[k] = v;
      ent.unlock("set");
    }
    else {                      // existing entry
      TMENT &ent = ((*i).second);
      ent.lock("set");
      unlock("set");
      ent.refresh();
      map_[k] = v;
      ent.unlock("set");
    }
  }

  void set_timedwait(const key_type &k, const V &v, ...);

  class Ptr {
    TMENT *p;

    mapped_type *vp;

  public:
    Ptr(const Ptr &ptr) : p(ptr.p), vp(ptr.vp) {
      XDEBUG(0, "Ptr copy CTOR");
      if (p)
        p->lock("ptr");
    }
    Ptr() : p(0), vp(0) {
      XDEBUG(0, "Ptr default CTOR");
    }
    Ptr(TMENT *ent, mapped_type *value) : p(ent), vp(value) {
      XDEBUG(0, "Ptr CTOR");
    }
    ~Ptr() {
      XDEBUG(0, "Ptr DTOR");
      // TODO#C: it would be better if we check the expiration
      //         and release it.
      if (p)
        p->unlock("ptr");
    }

    element_type *operator->() {
      if (vp)
        return vp;
      throw std::out_of_range("not found");
    }

    element_type &operator*() {
      if (vp)
        return *vp;
      throw std::out_of_range("not found");
    }

    operator bool() { return vp != 0; }
  };

  Ptr get(const key_type &k) {
    lock("get");
    typename tmap_type::iterator i = tmap_.find(k);
    if (i == tmap_.end()) {
      unlock("get");
      //throw std::out_of_range("not found");
      return Ptr();
    }

    TMENT &ent = (*i).second;
    ent.lock("get");
    unlock("get");
    time_t now = ::time(0);

    if (ent.expired(now, expiration_)) {
      // TODO: remove it?
      ent.unlock("get");
      //throw std::out_of_range("not found (expired)");
      return Ptr();
    }
    if (refresh_)
      ent.refresh();

    return Ptr(&ent, &map_[k]);
  }

};


template <typename K, typename V>
void *
TimerMap<K, V>::timer_main(void *arg)
{
  TimerMap *map = reinterpret_cast<TimerMap *>(arg);
  int oldstate;

  while (1) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
    sleep(map->timer_interval);
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

    if (!map->trylock("timer"))
      continue;

    XDEBUG(0, "timer: GC begin");
    map->remove_expired();
    XDEBUG(0, "timer: GC end");
    map->unlock("timer");
  }
  return (void *)0;
}

#endif  // TIMERMAP_H__
