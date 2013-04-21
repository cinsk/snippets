/* GNU obstack wrapper with some utilities
 * Copyright (C) 2003, 2004  Seong-Kook Shin <cinsky@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef OBSUTIL_H_
#define OBSUTIL_H_

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

#include <unistd.h>

#if 0
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H   1
#include <stdlib.h>
#include <stdint.h>
#endif  /* __APPLE__ */

#ifdef FAKEOBJS
#include "fakeobs.h"
#else
#include "obstack.h"
#endif

#ifndef obstack_chunk_alloc
# include <stdlib.h>
# define obstack_chunk_alloc     malloc
# define obstack_chunk_free      free
#endif

/* This indirect using of extern "C" { ... } makes Emacs happy */
#ifndef BEGIN_C_DECLS
# ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   };
# else
#  define BEGIN_C_DECLS
#  define END_C_DECLS
# endif
#endif /* BEGIN_C_DECLS */

BEGIN_C_DECLS


#define ZAP_PARAM_WARN(x)       ((void)x)

/*
 * We encapsulated GNU obstack to provide better interface,
 * All of GNU obstack functions and macros are provided in upper-case
 * names. For example, call OBSTACK_INIT() instead obstack_init(),
 * and use OBSTACK_CHUNK_SIZE() rather than obstack_chunk_size().
 *
 * obsutil wrappers -- that is, GNU obstack wrapper functions and macros
 * -- are the same as original GNU obstack functions in terms of argument
 * types and numbers.  It is much better to use obsutil wrappers because
 * it provides some error condition directly via return values.
 *
 * For example, the return type of obstack_init() is `void'. You will not
 * know weather the call was successful before installing obstack error
 * handler.  But OBSTACK_INIT() returns 0 if the call was successful,
 * otherwise it returns -1.
 *
 * All obsutils wrapper which returns 'int' type returns zero on success,
 * otherwise returns -1.  Note that some wrapper will not generate any
 * error. In this case, the return values are the same as the original
 * functions.
 *
 * All obsutils wrapper which returns some pointer type returns NULL on
 * failure.
 *
 * Documentation of OBSTACK_*() are not included here.
 * You should read info(1) document of glibc.
 * For example, on my RedHat Fedore Core 2, the document can be accessible
 * by issuing command:
 *   $ info libc memory "memory alloc" obstack
 *
 * As I said before, the return type of each OBSTACK_*() are slightly
 * different from original obstack_*().  You should read the codes here.
 */

#ifdef _PTHREAD

extern int *obs_errno_(void);
extern void obs_thread_init(void);

#define obs_errno   (*(obs_errno_()))

#else
extern int obs_errno;
#endif  /* _PTHREAD */

typedef void (*obstack_alloc_failed_handler_t)(void);
extern void obsutil_alloc_failed_handler(void);

#ifdef LEGACY
#define obsutil_init    obsutil_init
#define OBSTACK_ERROR   obs_has_error
#define OBSTACK_INIT    obs_init
#define OBSTACK_BEGIN   obs_begin
#define OBSTACK_ALLOC   obs_alloc
#define OBSTACK_OBJECT_SIZE(x)  obstack_object_size(x)
#define OBSTACK_FINISH(x)       obstack_finish(x)
#define OBSTACK_ROOM(x)         obstack_room(x)
#define OBSTACK_1GROW_FAST(s,v) obstack_1grow_fast((s), (v))
#define OBSTACK_INT_GROW(s,v)   obstack_int_grow((s), (v))
#define OBSTACK_INT_GROW_FAST(s,v) obstack_int_grow_fast((s), (v))
#define OBSTACK_FREE(s, o)      obstack_free((s), (o))
#define OBSTACK_COPY(s, a, z)   obs_copy((s), (a), (z))
#define OBSTACK_COPY0(s, a, z)  obs_copy0((s), (a), (z))
#define OBSTACK_BLANK(s, z)     obs_blank((s), (z))
#define OBSTACK_GROW(s, d, z)   obs_grow((s), (d), (z))
#define OBSTACK_GROW0(s, d, z)  obs_grow0((s), (d), (z))
#define OBSTACK_1GROW(s, c)     obs_1grow((s), (c))
#define OBSTACK_PTR_GROW(s, p)  obs_ptr_grow((s), (p))
#define OBSTACK_BLANK_FAST(s, z) obstack_blank_fast((s), (z))
#define OBSTACK_BASE(s)         obstack_base(s)
#define OBSTCK_NEXT_FREE(s)     obstack_next_free(s)
#define OBSTACK_ALIGNMENT_MASK(s)       obstack_alignment_mask(s)
#define OBSTACK_CHUNK_SIZE(s)   obstack_chunk_size(s)
#define OBSTACK_CAPACITY(s)         obstack_capacity(s)
#else
#define obs_object_size(x)      obstack_object_size(x)
#define obs_finish(x)           obstack_finish(x)
#define obs_room(x)             obstack_room(x)
#define obs_1grow_fast(s,c)     obstack_1grow_fast((s), (c))
#define obs_int_grow(s,v)       obstack_int_grow((s), (v))
#define obs_int_grow_fast(s,v)  obstack_int_grow_fast((s), (v))
#define obs_free(s, o)          obstack_free((s), (o))
#define obs_base(s)             obstack_base(s)
#define obs_blank_fast(s, z)    obstack_blank_fast((s), (z))
#define obs_next_free(s)        obstack_next_free(s)
#define obs_alignment_mask(s)   obstack_alignment_mask(s)
#define obs_chunk_size(s)       obstack_chunk_size(s)
#define obs_capacity(s)         obstack_capacity(s)
#endif  /* LEGACY */

/*
 * You should call obsutil_init() before calling any function or using
 * any macro in this header file.  obsutil_init() modifies the
 * global variable `obstack_alloc_failed_handler' to check errors.
 *
 * If you've used your own handler for `obstack_alloc_failed_handler',
 * you should call obsutil_alloc_failed_handler() in your own handler.
 *
 * If you're not sure, just call obsutil_init().
 */
extern obstack_alloc_failed_handler_t obsutil_init(void);

#define OBS_ERROR_CLEAR         do { obs_errno = 0; } while (0)
#define OBS_ERROR_SET           do { obs_errno = 1; } while (0)
#define OBS_ERROR               (obs_errno != 0)


/*
 * obs_has_error() is provided for the backward compatibility.
 * Developers must not use this function in new code.
 */
static __inline__ int
obs_has_error(void)
{
  return (obs_errno != 0);
}


static __inline__ int
obs_init(struct obstack *stack)
{
  OBS_ERROR_CLEAR;

  obstack_init(stack);
  if (obs_has_error())
    return -1;
  return 0;
}


static __inline__ int
obs_begin(struct obstack *stack, int size)
{
  OBS_ERROR_CLEAR;
  obstack_begin(stack, size);
  if (OBS_ERROR)
    return -1;
  return 0;
}


static __inline__ void *
obs_alloc(struct obstack *stack, int size)
{
  void *ptr;
  OBS_ERROR_CLEAR;
  ptr = obstack_alloc(stack, size);
  if (OBS_ERROR)
    return 0;
  return ptr;
}


static __inline__ void *
obs_copy(struct obstack *stack, const void *address, int size)
{
  void *ptr;
  OBS_ERROR_CLEAR;
  ptr = obstack_copy(stack, address, size);
  if (obs_errno)
    return 0;
  return ptr;
}


static __inline__ void *
obs_copy0(struct obstack *stack, const void *address, int size)
{
  void *ptr;
  OBS_ERROR_CLEAR;
  ptr = obstack_copy0(stack, address, size);
  if (obs_errno)
    return 0;
  return ptr;
}


static __inline__ int
obs_blank(struct obstack *stack, int size)
{
  OBS_ERROR_CLEAR;
  obstack_blank(stack, size);
  if (obs_errno)
    return -1;
  return 0;
}


static __inline__ int
obs_grow(struct obstack *stack, const void *data, int size)
{
  OBS_ERROR_CLEAR;
  obstack_grow(stack, data, size);
  if (obs_errno)
    return -1;
  return 0;
}


static __inline__ int
obs_grow0(struct obstack *stack, const void *data, int size)
{
  OBS_ERROR_CLEAR;
  obstack_grow0(stack, data, size);
  if (obs_errno)
    return -1;
  return 0;
}


static __inline__ int
obs_1grow(struct obstack *stack, char c)
{
  OBS_ERROR_CLEAR;
  obstack_1grow(stack, c);
  if (obs_errno)
    return -1;
  return 0;
}


static __inline__ int
obs_ptr_grow(struct obstack *stack, const void *data)
{
  OBS_ERROR_CLEAR;
  obstack_ptr_grow(stack, (void *)data);
  if (obs_errno)
    return -1;
  return 0;
}


extern size_t obstack_capacity(struct obstack *stack);

#ifndef NDEBUG
extern void obstack_dump(struct obstack *stack);
extern int obstack_belong(struct obstack *stack, void *ptr, size_t size);

#define OBSTACK_DUMP(stack)             obstack_dump(stack)
#define OBSTACK_BELONG(s, p, size)      obstack_belong(s, p, size)
#endif  /* NDEBUG */


/*
 * Store the current directory name in STACK as a growing object.
 * Note that the growing object is finished with a null character.
 */
static __inline__ char *
obs_getcwd_grow(struct obstack *stack)
{
  size_t capacity;
  char *cwd;
  int saved_errno = 0;

  assert(stack != NULL);
  assert(obs_object_size(stack) == 0);

  capacity = PATH_MAX;
  obs_blank(stack, capacity);

  while (1) {
    cwd = getcwd((char *)obs_base(stack), obs_object_size(stack));
    if (!cwd) {
      if (errno != ERANGE) {
        saved_errno = errno;
        break;
      }
      else {
        obs_blank(stack, PATH_MAX);
      }
    }
    else {
      obs_blank(stack, strlen(cwd) + 1 - obs_object_size(stack));
      break;
    }
  }
  if (!cwd) {
    obs_free(stack, obs_finish(stack));
    errno = saved_errno;
    return 0;                   /* Unrecoverable error occurred */
  }

  return cwd;
}


/*
 * Store the current directory into the given STACK.
 *
 * Returns a pointer to the current directory name.
 */
static __inline__ char *
obstack_getcwd(struct obstack *stack)
{
  char *p = obs_getcwd_grow(stack);
  return (p) ? (char *)obs_finish(stack) : 0;
}


/*
 * This function make the growing object finished, and create another
 * growing object that duplicates the previous one.
 *
 * It returns a pointer to the (first) finished object.
 */
static __inline__ void *
obs_dup_grow(struct obstack *stack)
{
  void *p;
  size_t size;

  size = obs_object_size(stack);
  if (size == 0)
    return 0;

  p = obs_finish(stack);
  if (!p)
    return 0;

  obs_grow(stack, p, size);
  return obs_base(stack);
}


/*
 * Analgous to OBSTACK_DUP_GROW() except it added additional null character
 * before finishing the growing object.  Note that the duplicated growing
 * object will not have additional null character.
 *
 * \code
 * char *p, *q;
 * OBSTACK_GROW(stack, "asdf", 4);
 * // At this point, STACK contains 4 bytes of data, "asdf".
 * p = OBSTACK_DUP_GROW0(stack);
 * // At this point, STACK have additional 4 bytes of data, "asdf".
 * // P points the first "asdf" that is terminated with null character.
 * q = OBSTACK_BASE(stack)
 * // At this point, Q points the second "asdf" that isn't terminated with
 * // a null character.
 * \endcode
 */
static __inline__ void *
obs_dup_grow0(struct obstack *stack)
{
  void *p;
  size_t size;

  size = obs_object_size(stack);
  if (size == 0)
    return 0;

  obs_1grow(stack, '\0');
  p = obs_finish(stack);
  if (!p)
    return 0;

  obs_grow(stack, p, size);
  return obs_base(stack);
}


//int OBSTACK_APPEND_GROW(struct obstack *stack, void *data, size_t size);
//int OBSTACK_APPEND_GROW0(struct obstack *stack, void *data, size_t size);


static __inline__ char *
obs_str_copy(struct obstack *stack, const char *s)
{
  return (char *)obs_copy0(stack, s, strlen(s));
}


static __inline__ char *
obs_str_copy2(struct obstack *stack, const char *s1, const char *s2)
{
  char *ret;
  int l1 = strlen(s1);
  int l2 = strlen(s2);

  ret = (char *)obs_alloc(stack, l1 + l2 + 1);
  if (!ret)
    return 0;

  strcpy(ret, s1);
  strcpy(ret + l1, s2);

  return ret;
}


static __inline__ char *
obs_str_copy3(struct obstack *stack,
              const char *s1, const char *s2, const char *s3)
{
  char *ret;
  int l1 = strlen(s1);
  int l2 = strlen(s2);
  int l3 = strlen(s3);

  ret = (char *)obs_alloc(stack, l1 + l2 + l3 + 1);
  if (!ret)
    return 0;

  strcpy(ret, s1);
  strcpy(ret + l1, s2);
  strcpy(ret + l1 + l2, s3);

  return ret;
}


static __inline__ int
obs_str_grow(struct obstack *stack, const char *s)
{
  return obs_grow0(stack, s, strlen(s));
}


static __inline__ char *
obs_str_grow2(struct obstack *stack, const char *s1, const char *s2)
{
  char *base;
  size_t size;
  int l1 = strlen(s1);
  int l2 = strlen(s2);

  size = obs_object_size(stack);
  obs_blank(stack, l1 + l2 + 1);
  if (obs_has_error())
    return 0;

  base = (char *)obs_base(stack) + size;
  strcpy(base, s1);
  strcpy(base + l1, s2);

  return base;
}


static __inline__ char *
obs_str_grow3(struct obstack *stack,
              const char *s1, const char *s2, const char *s3)
{
  char *base;
  size_t size;
  int l1 = strlen(s1);
  int l2 = strlen(s2);
  int l3 = strlen(s3);

  size = obs_object_size(stack);
  obs_blank(stack, l1 + l2 + l3 + 1);
  if (obs_has_error())
    return 0;
  base = (char *)obs_base(stack) + size;
  strcpy(base, s1);
  strcpy(base + l1, s2);
  strcpy(base + l1 + l2, s3);

  return base;
}


#ifndef NO_WCHAR_SUPPORT

#include <wchar.h>

static __inline__ int
obs_wgrow0(struct obstack *stack, const void *data, int size)
{
  int s;

  assert(size % sizeof(wchar_t) == 0);

  s = obs_object_size(stack);
  if (obs_grow(stack, data, size) < 0)
    return -1;

  if (obs_grow(stack, L"", sizeof(wchar_t)) < 0) {
    obs_blank(stack, -s);
    return -1;
  }
  return 0;
}


static __inline__ void *
obs_wcopy0(struct obstack *stack, const void *address, int size)
{
  assert(size % sizeof(wchar_t) == 0); /* check the alignment */

  if (obs_wgrow0(stack, address, size) < 0)
    return 0;

  return obs_finish(stack);
}


static __inline__ int
obs_1wgrow(struct obstack *stack, wchar_t c)
{
  return obs_grow(stack, &c, sizeof(c));
}


static __inline__ wchar_t *
obs_wstr_copy(struct obstack *stack, const wchar_t *s)
{
  return (wchar_t *)obs_wcopy0(stack, s, wcslen(s) * sizeof(wchar_t));
}

static __inline__ wchar_t *
obs_wstr_copy2(struct obstack *stack, const wchar_t *s1, const wchar_t *s2)
{
  wchar_t *p;
  size_t l1 = wcslen(s1);
  size_t l2 = wcslen(s2);

  p = (wchar_t *)obs_alloc(stack, (l1 + l2 + 1) * sizeof(wchar_t));
  if (!p)
    return 0;

  wcscpy(p, s1);
  wcscpy(p + l1, s2);

  return p;
}


static __inline__ wchar_t *
obs_wstr_copy3(struct obstack *stack,
               const wchar_t *s1, const wchar_t *s2, const wchar_t *s3)
{
  wchar_t *ret;
  int l1 = wcslen(s1);
  int l2 = wcslen(s2);
  int l3 = wcslen(s3);

  ret = (wchar_t *)obs_alloc(stack, (l1 + l2 + l3 + 1) * sizeof(wchar_t));
  if (!ret)
    return 0;

  wcscpy(ret, s1);
  wcscpy(ret + l1, s2);
  wcscpy(ret + l1 + l2, s3);

  return ret;
}


static __inline__ int
obs_wstr_grow(struct obstack *stack, const wchar_t *s)
{
  return obs_wgrow0(stack, s, wcslen(s) * sizeof(wchar_t));
}


static __inline__ int
obs_wstr_grow2(struct obstack *stack, const wchar_t *s1, const wchar_t *s2)
{
  wchar_t *base;
  size_t size;
  int l1 = wcslen(s1);
  int l2 = wcslen(s2);

  size = obs_object_size(stack);
  obs_blank(stack, (l1 + l2 + 1) * sizeof(wchar_t));
  if (obs_has_error())
    return -1;

  base = (wchar_t *)((char *)obs_base(stack) + size);
  wcscpy(base, s1);
  wcscpy(base + l1, s2);

  return 0;
}


static __inline__ int
obs_wstr_grow3(struct obstack *stack,
               const wchar_t *s1, const wchar_t *s2, const wchar_t *s3)
{
  wchar_t *base;
  size_t size;
  int l1 = wcslen(s1);
  int l2 = wcslen(s2);
  int l3 = wcslen(s3);

  size = obs_object_size(stack);
  obs_blank(stack, (l1 + l2 + l3 + 1) * sizeof(wchar_t));
  if (obs_has_error())
    return -1;
  base = (wchar_t *)((char *)obs_base(stack) + size);
  wcscpy(base, s1);
  wcscpy(base + l1, s2);
  wcscpy(base + l1 + l2, s3);

  return 0;
}
#endif  /* NO_WCHAR_SUPPORT */

extern char *obs_format(struct obstack *stack, const char *format, ...)
  __attribute__ ((format (printf, 2, 3)));

extern char *obs_grow_format(struct obstack *stack, const char *format, ...)
  __attribute__ ((format (printf, 2, 3)));


END_C_DECLS

#endif  /* OBSUTIL_H_ */
