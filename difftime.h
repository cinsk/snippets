/* -*-c++-*- */
#ifndef DIFFTIME_H__
#define DIFFTIME_H__

#include <sys/time.h>

/* This indirect using of extern "C" { ... } makes Emacs happy */
#ifndef BEGIN_C_DECLS
# ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
# else
#  define BEGIN_C_DECLS
#  define END_C_DECLS
# endif
#endif /* BEGIN_C_DECLS */


#define DIFFTIME_BEGIN  do { struct timeval b_egin____;             \
  gettimeofday(&b_egin____, 0);                                     \
  do {

#define END_DIFFTIME(store)  } while (0); { struct timeval end; \
    gettimeofday(&end, 0);                                      \
    (store) = diff_timeval(&end, &b_egin____); } } while (0)

BEGIN_C_DECLS

static __inline__ long
diff_timeval(const struct timeval *now, const struct timeval *old)
{
  long d;

  d = (now->tv_usec - old->tv_usec) / 1000;
  d += (now->tv_sec - old->tv_sec) * 1000;

  return d;
}

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199900L

struct df__ {
  struct timeval tv;
  int cont;
};

static __inline__ struct df__
df__gettime__(void)
{
  struct df__ tv;
  gettimeofday(&tv.tv, 0);
  tv.cont = 1;
  return tv;
}
#define DIFFTIME(store) for (struct df__ t_v__ = df__gettime__(), e_nd__; \
                             gettimeofday(&e_nd__.tv, 0),               \
                               (store) = diff_timeval(&e_nd__.tv, &t_v__.tv), \
                               t_v__.cont--; )
#endif

END_C_DECLS


#ifdef __cplusplus
#include <functional>

template <class adaptable>
class DiffTime {
private:
  adaptable adaptor;
  bool once;
  struct timeval begin;

public:
  DiffTime(long *pstore = 0)
    : adaptor(), once(true) {
    ::gettimeofday(&begin, 0);
  }

  DiffTime(adaptable &adaptor_)
    : adaptor(adaptor_), once(true)
  {
    ::gettimeofday(&begin, 0);
  }

  ~DiffTime() {
    struct timeval end;

    ::gettimeofday(&end, 0);

    adaptor(diff_timeval(&end, &begin));
  }

  operator bool() {
    bool ret = once;

    if (once)
      once = false;

    return ret;
  }
};


template <>
class DiffTime<long &> {
private:
  bool once;
  struct timeval begin;
  long &store;

public:
  DiffTime(long &pstore)
    : once(true), store(pstore) {
    ::gettimeofday(&begin, 0);
  }

  ~DiffTime() {
    struct timeval end;

    ::gettimeofday(&end, 0);

    store = diff_timeval(&end, &begin);
  }

  operator bool() {
    bool ret = once;

    if (once)
      once = false;

    return ret;
  }
};

typedef DiffTime<long &> DiffTimeStore;

#define DIFFTIME(var)   for (DiffTimeStore d__t__(var); d__t__; )

#endif  /* __cplusplus */

#endif  /* DIFFTIME_H__ */
