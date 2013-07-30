#ifndef TIMEDMAP_HH__
#define TIMEDMAP_HH__

#include <map>
#include <stdexcept>
#include <boost/shared_ptr.hpp>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>

#include "xerror.h"

//
// timedmap is a simple associative array that all keys will have
// same amount of life-time.  Once new (key, value) pair is registered,
// it will have a fixed life-time, and after the life-time expired,
// the pair will be automatically removed.
//
// To use timedmap, you need to declare the type name using
// TMAP_TYPE_DECL macro.  For example, if the key type is std::string,
// and the value type is Foo, and your timedmap type name is foomap,
// then you'll need to write:
//
//   TMAP_TYPE_DECL(foomap, std::string, Foo);
//
// Internally, timedmap utilize std::map, so your custom value type
// must provide default constructor which accepts no argument.  Once
// declared, you can create your own timedmap using the type name,
// 'foomap':
//
//   foomap fmap;
//
// To insert new value to 'fmap', you'll use 'set' method like this:
//
//   std::string key = "some-key-value";
//   fmap.set(key, Foo(...));
//
// To retrieve and use the value from 'fmap', you need a guard to
// protect the value so that the value should not be removed until
// you're done reading it:
//
//   TMAP_GET(foomap, fmap, std::string("some-key-value"), g) {
//     if (g) {
//       g->what();   // accessing Foo::what() method
//     }
//   }
//
// The last argument of TMAP_GET() macro is a kind of smart pointer,
// so that you can use -> or * operator to access the value's members.
// It also provides operator bool, so that you can test it whether the
// key, value pair exist.  Dereferencing non-existent value or expired
// value will raise std::out_of_range exception.
//
// Assume that the control is in TMAP_GET() macro block, and the key,
// value pair is not expied, then the pair will not be deleted until
// the control escapes from TMAP_GET() macro block even if the
// life-time of the pair is gone.
//
// TMAP_GET() macro is implemented using 'for' statement.  So use
// 'break' statement inside of TMAP_GET() macro block actually make
// the control to escape from TMAP_GET() macro.

template <class K, class V>
class timedmap {

  class Mutex {
    pthread_mutex_t mtx_;
    Mutex(const Mutex &);            // can't copied
    Mutex &operator=(const Mutex &); // can't assigned
  public:
    Mutex(int type = PTHREAD_MUTEX_ERRORCHECK) {
      pthread_mutexattr_t attr;

      pthread_mutexattr_init(&attr);
      pthread_mutexattr_settype(&attr, type);
      pthread_mutex_init(&mtx_, &attr);
      pthread_mutexattr_destroy(&attr);
    }

    ~Mutex() {
      int ret = pthread_mutex_destroy(&mtx_);
      if (ret)
        xerror(0, ret, "pthread_mutex_destroy failed");
    }

    void lock() {
      int ret = pthread_mutex_lock(&mtx_);
      if (ret)
        xerror(0, ret, "pthread_mutex_lock failed");
    }
    void unlock() {
      int ret = pthread_mutex_unlock(&mtx_);
      if (ret)
        xerror(0, ret, "pthread_mutex_unlock failed");
    }

    bool trylock() {
      int ret = pthread_mutex_trylock(&mtx_);

      if (!ret)
        return true;
      else {
        if (ret != EBUSY)
          xerror(0, ret, "pthread_mutex_trylock failed errno = %d", ret);
        return false;
      }
    }
  };

  // This is the actual value type of the internal map, timedmap::map_;
  struct TMENT {
    V val_;                     // user provided value
    boost::shared_ptr<Mutex> mtx_;
    time_t exp_;                // TMENT is invalid after exp_ (absolute time)

    TMENT(const TMENT &ent)
      : val_(ent.val_), mtx_(ent.mtx_), exp_(ent.exp_) {}

    TMENT() : val_(), mtx_(), exp_(0) {}

    explicit TMENT(const V &val, int duration)
      : val_(val), mtx_(new Mutex), exp_(time(0) + duration) {
    }
  };

  boost::shared_ptr<Mutex> mtx_; // mutex that protects timedmap::map_
  std::map<K,TMENT> map_;
  int duration_;                // default lifetime of new element in seconds

  typedef typename std::map<K, TMENT> impl_type;
  typedef typename std::map<K, TMENT>::iterator impl_iter_type;
  typedef typename std::map<K, TMENT>::const_iterator impl_const_iter_type;

  timedmap(const timedmap &);

  template<typename K2, typename V2> friend class timedmap_getter;

  static void *reaper_main(void *arg);
  pthread_t reaper_;

public:

  explicit timedmap(int duration = 5)
    : mtx_(new Mutex), map_(), duration_(duration) {
#ifndef NOREAP
    int ret = pthread_create(&reaper_, 0, reaper_main, (void *)this);
    if (ret != 0)
      xerror(0, ret, "pthread_create failed");
#endif  // NOREAP
  }

  ~timedmap() {
#ifndef NOREAP
    int ret;
    ret = pthread_cancel(reaper_);
    if (ret != 0)
      xerror(0, ret, "pthread_cancel failed");

    ret = pthread_join(reaper_, 0);
    if (ret != 0)
      xerror(0, ret, "pthread_join failed");
#endif  // NOREAP
  }

  int duration() const { return duration_; }

  void set(const K &k, const V &v, int duration = 0) {
    mtx_->lock();

    impl_iter_type i = map_.find(k);

    if (i == map_.end()) {
      map_[k] = TMENT(v, duration_);

      mtx_->unlock();
    }
    else {
      (*i).second.mtx_->lock();
      mtx_->unlock();

      (*i).second.val_ = v;
      (*i).second.exp_ = time(0) + (duration ? duration : duration_);
      (*i).second.mtx_->unlock();
    }
  }


  class timedmap_getter {
    TMENT *ent_;
    typename timedmap<K, V>::impl_iter_type iter_;

    mutable bool once_;
    int duration_;

    timedmap_getter(const timedmap_getter &);
    explicit timedmap_getter() : ent_(0), once_(true), duration_(0) {}

  public:
    explicit timedmap_getter(timedmap<K, V> &tmap, const K &key, int duration)
      : ent_(0), once_(true), duration_(duration) {
      tmap.mtx_->lock();
      iter_ = tmap.map_.find(key);

      if (iter_ != tmap.map_.end()) {
        ent_ = &((*iter_).second);
        ent_->mtx_->lock();

        if (ent_->exp_ < time(0)) {
          // remove the entry;
          ent_->mtx_->unlock();
          tmap.map_.erase(iter_);

          ent_ = 0;
        }

        tmap.mtx_->unlock();
      }
      else {
        tmap.mtx_->unlock();
      }
    }

    ~timedmap_getter() {
      if (ent_)
        ent_->mtx_->unlock();
    }

    operator bool() const {
      return (ent_ != 0);
    }

    bool once(void) const {
      if (once_) {
        once_ = false;
        return true;
      }
      return false;
    }

    void erase(void) {
      if (ent_)
        ent_->exp_ = 0;
    }

    void refresh(int duration = 0) {
      ent_->exp_ = time(0) + (duration ? duration : duration_);
    };

    V *operator->() {
      if (ent_)
        return &ent_->val_;
      throw std::out_of_range("not found");
    }

    V &operator*() {
      if (ent_)
        return ent_->val_;
      throw std::out_of_range("not found");
    }
  };

  typedef typename impl_type::size_type size_type;

  size_type size() const {
    mtx_->lock();
    size_type sz = map_.size();
    mtx_->unlock();
    return sz;
  }

  void erase(const K &k) {
    mtx_->lock();
    impl_iter_type i = map_.find(k);
    if (i != map_.end()) {
      TMENT &ent = (*i).second;
      ent.mtx_->lock();
      ent.mtx_->unlock();
      map_.erase(i);
    }
    mtx_->unlock();
  }

  bool exist(const K &k) const {
    bool ret = false;

    mtx_->lock();
    impl_const_iter_type i = map_.find(k);
    if (i != map_.end()) {
      const TMENT &ent = (*i).second;
      ent.mtx_->lock();

      if (!(ent.exp_ < time(0)))
        ret = true;

      ent.mtx_->unlock();
      mtx_->unlock();

      return ret;
    }
    mtx_->unlock();

    return ret;
  }

  // Do I need to implement iterators?  I don't think so.
  //
  // If I do, the timedmap instance should be locked until the
  // iterator goes out of scope.  This is not desired in regarding to
  // the performance.
};


template <class K, class V>
void *
timedmap<K, V>::reaper_main(void *arg)
{
  timedmap *tmap = (timedmap *)arg;
  int ret, cstate, ctype;
  xthread_set_name("reaper");
  xdebug(0, "reaper: start");

  ret = pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &ctype);
  if (ret != 0)
    xerror(0, ret, "pthread_setcanceltype failed");

  while (1) {
    // TODO: cancel state
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cstate);
    sleep(tmap->duration_ * 2);
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cstate);

    if (tmap->mtx_->trylock()) {
      // this loop will traverse all elements in timedmap::map_,
      // since, the traversal will be done with the mutex locked, it
      // should be finished ASAP.
      xdebug(0, "reaper: harvesting begin");
      typename timedmap::impl_iter_type i = tmap->map_.begin();
      while (i != tmap->map_.end()) {
        TMENT &ent = (*i).second;
        if (ent.mtx_->trylock()) {
          if (ent.exp_ < time(0)) {
            ent.mtx_->unlock();
            xdebug(0, "reaper: remove key, %d", (*i).first);
            tmap->map_.erase(i++);
          }
          else
            ent.mtx_->unlock();
        }
        else           // the element is locked in elsewhere, skipped.
          i++;
      }
      tmap->mtx_->unlock();
      xdebug(0, "reaper: harvesting end");
    }
  }
  return 0;
}


#define TMAP_TYPE_DECL(mtype, ktype, vtype)                             \
  typedef timedmap<ktype, vtype> mtype;                                 \
  typedef timedmap<ktype, vtype>::timedmap_getter mtype##getter;


#define TMAP_GET(mtype, tmap, key, gter)     \
  for (mtype##getter gter((tmap), (key), (tmap).duration()); (gter).once(); )


#endif  // TIMEDMAP_HH__
