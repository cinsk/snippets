/* -*-c++-*- */
#ifndef TLSTORE_HH__
#define TLSTORE_HH__

#include <pthread.h>
#include <stdexcept>
#include <string.h>
#include "xerror.h"

const std::string error_string(int errnum)
{
  char buf[BUFSIZ];
  if (strerror_r(errnum, buf, BUFSIZ) == 0)
    return std::string(buf);
  else {
    snprintf(buf, BUFSIZ, "strerror_r(%d) failed", errnum);
    return std::string(buf);
  }
}


template <class T>
class tlstore {
  pthread_key_t key_;
  T *tls_;

  static void delete_key(void *p) {
    T *tlsp = (T *)p;
    delete tlsp;
  }

  tlstore(const tlstore &);

public:
  tlstore() {
    int ret = pthread_key_create(&key_, delete_key);
    if (ret)
      throw std::runtime_error(error_string(ret));
  }

  ~tlstore() {
    int ret = pthread_key_delete(key_);
    if (ret)
      xerror(0, ret, "pthread_key_delete() failed");
  }

  T *get() {
    return (T *)pthread_getspecific(key_);
  }

  void reset(T *newval) {
    void *p = pthread_getspecific(key_);
    if (p)
      delete_key(p);

    int ret = pthread_setspecific(key_, (void *)newval);
    if (ret)
      throw std::runtime_error(error_string(ret));
  }

  operator bool() {
    return get() != 0;
  }

  T *operator->()             { return get(); }
  const T *operator->() const { return get(); }
  T &operator*()              { return *get(); }
  const T &operator*() const  { return *get(); }
};

#endif  /* TLSTORE_HH__ */
