/* -*-c++-*- */

#include <map>
#include <queue>
#include <functional>
#include <algorithm>
#include <stdexcept>

#include <string>

#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "xerror.h"

template <typename K, typename V>
class timedmap {
  struct entry {
    time_t tm_;
    K key_;

    entry(int expiration, const K &key)
      : tm_(time(0) + expiration), key_(key) {}
    // entry(time_t t, const K &key) : tm_(t), key_(key) {}

    bool operator>(const entry &rhs) const { return (tm_ > rhs.tm_); }
    bool operator>=(const entry &rhs) const { return (tm_ >= rhs.tm_); }
    bool operator<=(const entry &rhs) const { return (tm_ >= rhs.tm_); }
    bool operator<(const entry &rhs) const { return (tm_ < rhs.tm_); }

    bool operator==(const entry &rhs) const { return (tm_ == rhs.tm_); }
    bool operator!=(const entry &rhs) const { return (tm_ == rhs.tm_); }
  };

  struct greater : std::binary_function<entry, entry, bool> {
    bool operator()(const entry &x, const entry &y) { return x > y; }
  };

  typedef typename std::map<K, V> impl_type;
  typedef std::priority_queue<entry, std::vector<entry>,
                              greater> queue_type;
  mutable impl_type map_;
  mutable queue_type queue_;

  size_t ref_;
  size_t expiration_;

  void reap() const {
    xdebug(0, "reap: ref-count(%zd)", ref_);
    if (ref_ > 0)
      return;

    while (!queue_.empty()) {
      if (queue_.top().tm_ < time(0)) {
        typename impl_type::iterator i = map_.find(queue_.top().key_);
        if (i != map_.end()) {
          xdebug(0, "removing key(%d), expired", (*i).first);
          queue_.pop();
          map_.erase(i);
        }
        else
          xdebug(0, "huh? key(%d) not exist before expiration", (*i).first);
      }
      else
        break;
    }
  }

public:
  timedmap(const timedmap &m)
    : map_(m.map_), queue_(m.queue_), ref_(0), expiration_(m.expiration_) {}
  timedmap(int expiration) : map_(), queue_(), ref_(0), expiration_(1) {}

  class iterator_ {
    typedef typename timedmap::impl_type::iterator impl_iter;
    impl_iter pos_;
    timedmap<K, V> &map_;
  public:
    iterator_(const iterator_ &i) : pos_(i.pos_), map_(i.map_) {
      ++map_.ref_;
    }

    iterator_(impl_iter pos, timedmap<K, V> &map)
      : pos_(pos), map_(map) {
      ++map_.ref_;
    }

    ~iterator_() { --map_.ref_; }

    bool operator==(const iterator_ &rhs) { return pos_ == rhs.pos_; }
    bool operator!=(const iterator_ &rhs) { return pos_ != rhs.pos_; }

    void operator++() { ++pos_; }
    V &operator*() {

      return (*pos_).second.val_;
    }
  };

  const V &at(const K &key) const {
    reap();

    typename impl_type::iterator i = map_.find(key);
    if (i == map_.end())
      throw std::out_of_range("not found");
    return (*i).second;
  }

  V &operator[](const K &key) {
    reap();
    queue_.push(entry(expiration_, key));
    return map_[key];
  }

  //typedef class iterator iterator_type;
  iterator_ begin() {
    reap();
    return timedmap::iterator_(map_.begin(), *this);
  }

  iterator_ end() {
    reap();
    return iterator_(map_.end(), *this);
  }

  typedef iterator_ iterator;

};

#if 0
template <typename K, typename V>
class timedmap {
  struct TMENT {
    time_t tm_;
    V val_;

    TMENT() : tm_(time(0)), val_() {}
    TMENT(const TMENT &ent) : tm_(ent.tm_), val_(ent.val_) {}
    TMENT(const V &val) : tm_(time(0)), val_(val) {
    }

    void refresh() { tm_ = time(0); }
    bool expired(time_t expiration) {
      time_t now = ::time(0);
      return (now - tm_) > expiration;
    }
  };

  typedef std::map<K, TMENT> impl_type;
  typedef std::priority_queue<K, std::vector<
  impl_type map_;

public:
  timedmap(const timedmap &m) : map_(m) {}
  timedmap() : map_() {}

  typedef V value_type;
  typedef V *pointer;
  typedef const V* const_pointer;
  typedef V &reference;
  typedef const V &const_reference;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  V &at(const K &key) {
    try {
      TMENT &ent = map_[key];
      return ent.val_;
    }
    catch (...) {
      throw;
    }
  }

  V &operator[](const K &key) {
    typename impl_type::iterator i = map_.find(key);
    if (i != map_.end()) {
      TMENT &ent = (*i).second;
      return ent.val_;
    }
    else {
      map_[key] = TMENT();
      return map_[key].val_;
    }
  }

  class iterator {
    typename impl_type::iterator pos_;
    timedmap<K, TMENT> &map_;
  public:
    iterator(typename impl_type::iterator pos, timedmap<K, V> &map)
      : pos_(pos), map_(map) {}

    bool operator==(const iterator &rhs) { return pos_ == rhs.pos_; }
    bool operator!=(const iterator &rhs) { return pos_ != rhs.pos_; }

    void operator++() { ++pos_; }
    V &operator*() {
      return (*pos_).second.val_;
    }
  };

  typedef iterator iterator_type;

  iterator_type begin() { return iterator(map_.begin(), *this); }
  iterator_type end() { return iterator(map_.end(), *this); }

};


#define XDEBUG  xdebug

#define M_LOCK(m)       (m ? (m)->lock(__FILE__, __LINE__, __PRETTY_FUNCTION__) : 0)
#define M_UNLOCK(m)     (m ? (m)->unlock(__FILE__, __LINE__, __PRETTY_FUNCTION__) : 0)
#define M_TRYLOCK(m)    (m ? (m)->trylock(__FILE__, __LINE__, __PRETTY_FUNCTION__) : false)


template <typename K, typename V>
class TimedMap {
  class MUTEX {
    ::pthread_mutex_t lck_;
    std::string prefix_;
    std::string file_;
    int line_;
    std::string func_;
    bool noabort_;

    MUTEX(const MUTEX &m);

  public:
    MUTEX(int type = PTHREAD_MUTEX_DEFAULT, bool noabort = false,
          const std::string &prefix = "")
      : prefix_(prefix), file_(), line_(0), func_(), noabort_(noabort) {
      xdebug(0, "MUTEX(%s) CTOR", prefix_.c_str());
      ::pthread_mutexattr_t attr;
      ::pthread_mutexattr_init(&attr);
      ::pthread_mutexattr_settype(&attr, type);
      ::pthread_mutex_init(&lck_, &attr);
      ::pthread_mutexattr_destroy(&attr);
    }

    ~MUTEX() {
      xdebug(0, "MUTEX(%s) DTOR", prefix_.c_str());
      int ret = ::pthread_mutex_destroy(&lck_);
      if (ret) {
        xerror(0, ret, "pthread_mutex_destroy failed");
        if (ret == EBUSY) {
          xerror(0, 0, "%s:%d: locked at %s",
                 file_.c_str(), line_, func_.c_str());
        }
        if (!noabort_)
          abort();
      }
    }

    void debug(const char *filename, long lineno, const char *func) {
      line_ = 0;

      file_ = std::string(filename ? filename : "*NIL*");
      line_ = lineno;
      func_ = std::string(func ? func : "*NIL*");
    }

    int lock(const char *filename, long lineno, const char *func) {
      xdebug(0, "%s:%ld: LOCK(%s) from %s",
             filename, lineno, prefix_.c_str(), func);
      int ret = pthread_mutex_lock(&lck_);
      if (ret) {
        xerror(0, ret, "pthread_mutex_lock() failed");
        abort();
      }
      else
        debug(filename, lineno, func);
      return ret;
    }

    int unlock(const char *filename, long lineno, const char *func) {
      xdebug(0, "%s:%ld: UNLOCK(%s) from %s",
             filename, lineno, prefix_.c_str(), func);
      debug(0, lineno, 0);
      int ret = pthread_mutex_unlock(&lck_);
      if (ret) {
        xerror(0, ret, "pthread_mutex_unlock() failed");
        abort();
      }
      return ret;
    }

    bool trylock(const char *filename, long lineno, const char *func) {
      xdebug(0, "%s:%ld: TRYLOCK from %s", filename, lineno, func);
      int ret = pthread_mutex_trylock(&lck_);
      if (ret != 0 && ret != EBUSY) {
        xerror(0, ret, "pthread_mutex_trylock() failed");
        abort();
      }
      if (!ret)                 // locked
        debug(filename, lineno, func);
      return ret ? false : true;
    }
  };

  struct TMENT {
    time_t tm_;
    boost::shared_ptr<MUTEX> m_;
    V val_;

    TMENT() : tm_(time(0)), m_(), val_() {}
    TMENT(const TMENT &ent) : tm_(ent.tm_), m_(ent.m_), val_(ent.val_) {}

    TMENT(const V &val)
      : tm_(time(0)), m_(new MUTEX(PTHREAD_MUTEX_RECURSIVE)), val_(val) {
    }

    void refresh() { tm_ = time(0); }
    bool expired(time_t expiration) {
      time_t now = ::time(0);
      return (now - tm_) > expiration;
    }
  };

  typedef std::map<K, TMENT> map_type;

  typedef typename map_type::key_type key_type;
  typedef typename map_type::mapped_type mapped_type;
  typedef typename map_type::value_type value_type;

  TimedMap(const TimedMap &);

  map_type map_;
  MUTEX mlock_;
  int expiration_;
  bool refresh_;

public:
  explicit TimedMap(int expiration)
    : map_(), mlock_(PTHREAD_MUTEX_RECURSIVE, true, "TimedMap"),
      expiration_(expiration), refresh_(false) {
  }

  ~TimedMap() {
    // clear();
  }

  void clear() {
    M_LOCK(&mlock_);
    typename map_type::iterator i = map_.begin();
    while (i != map_.end()) {
      TMENT &ent = (*i).second;
      M_LOCK(ent.m_);
      M_UNLOCK(ent.m_);
      map_.erase(i++);
    }
    M_UNLOCK(&mlock_);
  }

  class Ptr {
    TMENT *ent_;

    mutable bool owned_;

    bool yield() const {
      if (!owned_) {
        xerror(1, 0, "Ptr can't yield ownership when it doesn't");
        abort();
      }
      owned_ = false;
      return true;
    }

    Ptr &operator=(const Ptr &ptr);

  public:
    Ptr(const Ptr &ptr) : ent_(ptr.ent_) {
      //owned_ = ptr.yield();
      if (ent_)
        M_LOCK(ent_->m_);
    }
    Ptr() : ent_(0), owned_(false) {}

    Ptr(TMENT *ent) : ent_(ent), owned_(true) {
    }

    ~Ptr() {
      xdebug(0, "Ptr DTOR: owned(%d)", (int)owned_);

      if (ent_)
        M_UNLOCK(ent_->m_);
    }

    V *operator->() {
      if (!owned_)
        throw std::out_of_range("not found");
      return &(ent_->val_);
    }

    V &operator*() {
      if (!owned_)
        throw std::out_of_range("not found");
      return ent_->val_;
    }

    operator bool() {
      if (owned_)
        return true;
      return false;
    }
  };


  void set(const key_type &k, const V &v) {
    M_LOCK(&mlock_);
    typename map_type::iterator i = map_.find(k);
    if (i == map_.end()) {
      map_[k] = TMENT(v);
    }
    else {
      TMENT &ent = (*i).second;
      M_LOCK(ent.m_);
      ent.refresh();
      ent.val_ = v;
      M_UNLOCK(ent.m_);
    }
    M_UNLOCK(&mlock_);
  }

  Ptr get(const key_type &k) {
    M_LOCK(&mlock_);

    typename map_type::iterator i = map_.find(k);

    if (i == map_.end()) {
      xdebug(0, "TimedMap::get() key not found");
      M_UNLOCK(&mlock_);
      return Ptr();
    }

    TMENT &ent = (*i).second;

    M_LOCK(ent.m_);

    if (ent.expired(expiration_)) {
      xdebug(0, "TimedMap::get() key expired");
      M_UNLOCK(ent.m_);
      return Ptr();
    }
    else if (refresh_)
      ent.refresh();

    M_UNLOCK(&mlock_);

    // Intentionally leave the 'ent' as locked state.
    return Ptr(&ent);
  }
};
#endif  // 0
