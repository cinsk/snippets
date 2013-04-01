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


#define DIFFTIME        do { struct timeval tv__old__, tv__now__;       \
  gettimeofday(&tv__old__, NULL);

#define END_DIFFTIME(v)    gettimeofday(&tv__now__, NULL);   \
  (v) = diff_timeval(&tv__now__, &tv__old__);                \
  } while (0)


BEGIN_C_DECLS

static __inline__ long
diff_timeval(const struct timeval *now, const struct timeval *old)
{
  long d;

  d = (now->tv_usec - old->tv_usec) / 1000;
  d += (now->tv_sec - old->tv_sec) * 1000;

  return d;
}

END_C_DECLS

#ifdef __cplusplus

template <class adaptable>
class DiffTime {
  adaptable f;
  struct timeval old;

public:
  DiffTime() { ::gettimeofday(&old, 0); }
  ~DiffTime() {
    struct timeval now;
    ::gettimeofday(&now, 0);
    f(::diff_timeval(&now, &old));
  }
};

#endif  /* __cplusplus */

#endif  /* DIFFTIME_H__ */
