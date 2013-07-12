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
// To retrive a value (e.g. where key is 0), you need a lock.  There
// are two interface for retrival:
//
// 1)
//   TIMERMAP_SYNC(TMAP, m) {
//     try {
//       boost::shared_ptr<Foo> ptr = m.get(0);
//       ptr->SOME_OPERATION();
//     }
//     catch (std::out_of_range &e) {
//       // the key, 0 is not found in the map
//       break;
//     }
//   }
//
// TIMERMAP_SYNC will lock the whole map (for now, I suppose), and
// you can safely use the (key, value) pair.  In this block, the
// pair will be not expired Even if EXPIRATION time reached.
//
// 2)
//   try {
//     boost::shared_ptr<Foo> ptr = m.getptr(0);
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

template <typename K, typename V>
class TimerMap {

  typedef std::map<K, time_t> tmap_type;
  typedef std::map<K, V> map_type;

  tmap_type tmap_;
  map_type map_;

  TimerMap(const TimerMap &);

  pthread_t timer_thread;
  pthread_mutex_t lock_;

  V &operator[](const K &k) {
    return map_[k];
  }

  int expiration_;
  bool refresh_;

public:
  static void *timer_main(void *arg);
  static const int timer_interval = 1;

  typedef typename std::map<K, V>::key_type key_type;
  typedef typename std::map<K, V>::mapped_type mapped_type;
  typedef typename std::map<K, V>::value_type value_type;

  explicit TimerMap(int expiration, bool refresh = false) : map_(), expiration_(expiration), refresh_(refresh) {
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
    // xdebug(0, "timer_thread returns %p", pret);
    ::pthread_mutex_destroy(&lock_);
  }

  //key_type foo(const key_type &k) { return k; }

  void set(const key_type &k, const V &v) {
    pthread_mutex_lock(&lock_);
    tmap_[k] = ::time(0);
    map_[k] = v;
    pthread_mutex_unlock(&lock_);
  }

  V &get(const key_type &k) {
#ifndef NDEBUG
    if (::pthread_mutex_trylock(&lock_) != 0) {
      abort();
    }
    ::pthread_mutex_unlock(&lock_);
#endif  // NDEBUG

    time_t now = ::time(0);

    typename tmap_type::iterator i = tmap_.find(k);
    if (i == tmap_.end())
      throw std::out_of_range("not found");

    if (now - (*i).second > expiration_) {
      throw std::out_of_range("not found (expired)");
    }
    else {
      if (refresh_)
        (*i).second = ::time(0);
      return map_[k];
    }
  }

  ::pthread_mutex_t &lock__() { return lock_; }

  class Ptr {
    ::pthread_mutex_t &lock_;
    bool evaluated;
    V *value_;

  public:
    Ptr(const Ptr &oldptr)
      : lock_(oldptr.lock_), evaluated(oldptr.evaluated), value_(oldptr.value_) {
      xdebug(0, "Ptr2::LOCK");
      ::pthread_mutex_lock(&lock_);;
    }

    Ptr(::pthread_mutex_t &lck, TimerMap &m, const key_type &k)
      : lock_(lck), evaluated(false), value_(0) {
      xdebug(0, "Ptr::LOCK");
      ::pthread_mutex_lock(&lock_);;
      try {
        value_ = &m.get(k);
      }
      catch (std::out_of_range &e) {
        value_ = 0;

        // Warning, if m.get() raises an exception, then the
        // destructor of this class instance never be called, since
        // the instance is not fully constructed.  Thus, we need to
        // unlock the mutex here.
        ::pthread_mutex_unlock(&lock_);;

        throw;
      }
    }
    ~Ptr() {
      xdebug(0, "Ptr::UNLOCK");
      ::pthread_mutex_unlock(&lock_);
    }

    operator bool() {
      bool ret = !evaluated;
      if (!evaluated) evaluated = true;
      return ret;
    }

#if 0
    typename V::element_type &get() {
      return *(*value_).get();
    }

    typename V::element_type &operator()() {
      return *(*value_).get();
    }
#endif  // 0

    typename V::element_type *operator->() {
      return (*value_).get();
    }
  };

  Ptr getptr(const key_type &k) {
    return Ptr(lock_, *this, k);
  }

  class Lock {
    ::pthread_mutex_t &lock_;
    Lock(const Lock &);
    bool evaluated;

  public:
    Lock(::pthread_mutex_t &lck) : lock_(lck), evaluated(false) {
      xdebug(0, "LOCK");
      ::pthread_mutex_lock(&lock_);;
    }
    ~Lock() {
      xdebug(0, "UNLOCK");
      ::pthread_mutex_unlock(&lock_);
    }

    operator bool() {
      bool ret = !evaluated;
      if (!evaluated) evaluated = true;
      return ret;
    }
  };
};


template <typename K, typename V>
void *
TimerMap<K, V>::timer_main(void *arg)
{
  TimerMap *map = reinterpret_cast<TimerMap *>(arg);
  int oldstate;
  time_t now;

  xdebug(0, "timer: begin");

  while (1) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
    sleep(map->timer_interval);
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

    xdebug(0, "timer: check start");
    if (pthread_mutex_trylock(&map->lock_) == 0) {
      now = ::time(0);

      typename tmap_type::iterator i = map->tmap_.begin();
      while (i != map->tmap_.end()) {
        // xdebug(0, "timer: now(%ld) checking %d timer (%ld)\n", now, (*i).first, (*i).second);
        if ((now - (*i).second) > map->expiration_) {
          typename map_type::iterator j = map->map_.find((*i).first);
          if (j != map->map_.end()) {
            //xdebug(0, "timer: deleting %d", (*i).first);
            map->map_.erase(j);
          }
          else {
            //xdebug(0, "timer: index %d not found", (*i).first);
          }

          map->tmap_.erase(i++);
        }
        else
          ++i;
      }
      pthread_mutex_unlock(&map->lock_);
    }

  }
  return (void *)0;
}

#define TIMERMAP_SYNC(type, m)        for (type::Lock l((m).lock__()); l; )

#endif  // TIMERMAP_H__
