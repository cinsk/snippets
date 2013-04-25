/* -*-c++-*- */
#ifndef DIFFTIME_H__
#define DIFFTIME_H__

/*
 * Measuring Time Difference
 * Copyright (C) 2013  Seong-Kook Shin <cinsky@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 */

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


/*
 * Usage:
 *
 * In ANSI C code (upto C90):
 *
 *   long elapsed;
 *   DIFFTIME_BEGIN {
 *     your_code;
 *     ...;
 *   } END_DIFFTIME(elapsed);
 *   printf("elapsed: %ld\n", elapsed);
 *
 * In ISO C code (C99) and C++:
 *
 *   long elapsed;
 *   DIFFTIME(elasped) {
 *     your_code;
 *     ...;
 *   }
 *   printf("elapsed: %ld\n", elapsed);
 *
 */
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
                             t_v__.cont--;                              \
                             gettimeofday(&e_nd__.tv, 0),               \
                             (store) = diff_timeval(&e_nd__.tv, &t_v__.tv) \
                             )
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
