/*
 * malloc debugger
 * Copyright (C) 2004  Seong-Kook Shin <cinsky@gmail.com>
 */
#ifndef XMALLOC_H_
#define XMALLOC_H_

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stddef.h>

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

BEGIN_C_DECLS

#define XMEM_STAT
#define XMEM_REDZONE

#ifndef NDEBUG
extern void *xmalloc(size_t size);
extern void *xcalloc(size_t nmemb, size_t size);
extern void *xrealloc(void *ptr, size_t size);
extern void xfree(void *ptr);
extern void xmemstat(memstat_t *ms);
extern long xmemopt(int option, ...);
#else
# define xmalloc(size)          malloc(size)
# define xcalloc(nmemb, size)   calloc(nmemb, size)
# define xrealloc(ptr, size)    realloc(ptr, size)
# define xfree(ptr)             free(ptr)
# define xmemstat(ms)           ((void)0)
# define xmemopt(opt, val)      ((long)-1)
#endif /* NDEBUG */

struct memstat_ {
  int malloc_called;
  int calloc_called;
  int realloc_called;
  int free_called;

  size_t cur_size;
  size_t max_size;
  ssize_t limit;
};
typedef struct memstat_ memstat_t;

enum {
  X_SETLIM,                     /* Set memory limit */
  X_GETLIM,                     /* Get memory limit */
  X_SETFH,                      /* Set the failed handler */
  X_GETFH,                      /* Get the failed handler */
  X_SETES,                      /* Set the error stream */
  X_GETES,                      /* Get the error stream */
};

END_C_DECLS

#endif /* XMALLOC_H_ */
