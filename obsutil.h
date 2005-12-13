/* $Id$ */
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

#include "obstack.h"

#ifndef obstack_chunk_alloc
# include <stdlib.h>
# define obstack_chunk_alloc     malloc
# define obstack_chunk_free      free
#endif

/* This indirect writing of extern "C" { ... } makes XEmacs happy */
#ifndef BEGIN_C_DECLS
# define BEGIN_C_DECLS   extern "C" {
# define END_C_DECLS      }
#endif  /* BEGIN_C_DECLS */

#ifdef __cplusplus
BEGIN_C_DECLS
#endif


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

extern int obstack_errno_;

typedef void (*obstack_alloc_failed_handler_t)(void);
extern void obsutil_alloc_failed_handler(void);

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

#define OBS_ERROR_CLEAR         do { obstack_errno_ = 0; } while (0)
#define OBS_ERROR_SET           do { obstack_errno_ = 1; } while (0)
#define OBS_ERROR               (obstack_errno_ != 0)


/*
 * OBSTACK_ERROR() is provided for the backward compatibility.
 * Developers must not use this function in new code.
 */
static __inline__ int
OBSTACK_ERROR(void)
{
  return (obstack_errno_ != 0);
}


static __inline__ int
OBSTACK_INIT(struct obstack *stack)
{
  obstack_init(stack);
  if (OBS_ERROR)
    return -1;
  return 0;
}


static __inline__ int
OBSTACK_BEGIN(struct obstack *stack, int size)
{
  OBS_ERROR_CLEAR;
  obstack_begin(stack, size);
  if (OBS_ERROR)
    return -1;
  return 0;
}


static __inline__ void *
OBSTACK_ALLOC(struct obstack *stack, int size)
{
  void *ptr;
  OBS_ERROR_CLEAR;
  ptr = obstack_alloc(stack, size);
  if (OBS_ERROR)
    return 0;
  return ptr;
}


static __inline__ void *
OBSTACK_COPY(struct obstack *stack, const void *address, int size)
{
  void *ptr;
  OBS_ERROR_CLEAR;
  ptr = obstack_copy(stack, address, size);
  if (obstack_errno_)
    return 0;
  return ptr;
}


static __inline__ void *
OBSTACK_COPY0(struct obstack *stack, void *address, int size)
{
  void *ptr;
  OBS_ERROR_CLEAR;
  ptr = obstack_copy0(stack, address, size);
  if (obstack_errno_)
    return 0;
  return ptr;
}


static __inline__ void
OBSTACK_FREE(struct obstack *stack, void *object)
{
  obstack_free(stack, object);
}


static __inline__ int
OBSTACK_BLANK(struct obstack *stack, int size)
{
  OBS_ERROR_CLEAR;
  obstack_blank(stack, size);
  if (obstack_errno_)
    return -1;
  return 0;
}


static __inline__ int
OBSTACK_GROW(struct obstack *stack, void *data, int size)
{
  OBS_ERROR_CLEAR;
  obstack_grow(stack, data, size);
  if (obstack_errno_)
    return -1;
  return 0;
}


static __inline__ int
OBSTACK_GROW0(struct obstack *stack, void *data, int size)
{
  OBS_ERROR_CLEAR;
  obstack_grow0(stack, data, size);
  if (obstack_errno_)
    return -1;
  return 0;
}


static __inline__ int
OBSTACK_1GROW(struct obstack *stack, char c)
{
  OBS_ERROR_CLEAR;
  obstack_1grow(stack, c);
  if (obstack_errno_)
    return -1;
  return 0;
}


static __inline__ int
OBSTACK_PTR_GROW(struct obstack *stack, const void *data)
{
  OBS_ERROR_CLEAR;
  obstack_ptr_grow(stack, data);
  if (obstack_errno_)
    return -1;
  return 0;
}


static __inline__ int
OBSTACK_INT_GROW(struct obstack *stack, int data)
{
  OBS_ERROR_CLEAR;
  obstack_int_grow(stack, data);
  if (obstack_errno_)
    return -1;
  return 0;
}


static __inline__ void *
OBSTACK_FINISH(struct obstack *stack)
{
  return obstack_finish(stack);
}


static __inline__ int
OBSTACK_OBJECT_SIZE(struct obstack *stack)
{
  return obstack_object_size(stack);
}


static __inline__ int
OBSTACK_ROOM(struct obstack *stack)
{
  return obstack_room(stack);
}


static __inline__ void
OBSTACK_1GROW_FAST(struct obstack *stack, char c)
{
  obstack_1grow_fast(stack, c);
}


static __inline__ void
OBSTACK_INT_GROW_FAST(struct obstack *stack, int data)
{
  obstack_int_grow_fast(stack, data);
}


static __inline__ void
OBSTACK_BLANK_FAST(struct obstack *stack, int size)
{
  obstack_blank_fast(stack, size);
}


static __inline__ void *
OBSTACK_BASE(struct obstack *stack)
{
  return obstack_base(stack);
}


static __inline__ void *
OBSTACK_NEXT_FREE(struct obstack *stack)
{
  return obstack_next_free(stack);
}


static __inline__ int
OBSTACK_ALIGNMENT_MASK(struct obstack *stack)
{
  return obstack_alignment_mask(stack);
}


static __inline__ int
OBSTACK_CHUNK_SIZE(struct obstack *stack)
{
  return obstack_chunk_size(stack);
}


#ifndef NDEBUG
void OBSTACK_DUMP(struct obstack *stack);
int OBSTACK_BELONG(struct obstack *stack, void *ptr, size_t size);
#endif  /* NDEBUG */


/*
 * Store the current directory name in STACK as a growing object.
 * Note that the growing object is finished with a null character.
 */
static __inline__ char *
OBSTACK_GETCWD_GROW(struct obstack *stack)
{
  size_t capacity;
  char *cwd;
  int saved_errno = 0;

  assert(stack != NULL);
  assert(OBSTACK_OBJECT_SIZE(stack) == 0);

  capacity = PATH_MAX;
  OBSTACK_BLANK(stack, capacity);

  while (1) {
    cwd = getcwd((char *)OBSTACK_BASE(stack), OBSTACK_OBJECT_SIZE(stack));
    if (!cwd) {
      if (errno != ERANGE) {
        saved_errno = errno;
        break;
      }
      else {
        OBSTACK_BLANK(stack, PATH_MAX);
      }
    }
    else {
      OBSTACK_BLANK(stack, strlen(cwd) + 1 - OBSTACK_OBJECT_SIZE(stack));
      break;
    }
  }
  if (!cwd) {
    OBSTACK_FREE(stack, OBSTACK_FINISH(stack));
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
OBSTACK_GETCWD(struct obstack *stack)
{
  char *p = OBSTACK_GETCWD_GROW(stack);
  return (p) ? (char *)OBSTACK_FINISH(stack) : 0;
}


/*
 * This function make the growing object finished, and create another
 * growing object that duplicates the previous one.
 *
 * It returns a pointer to the (first) finished object.
 */
static __inline__ void *
OBSTACK_DUP_GROW(struct obstack *stack)
{
  void *p;
  size_t size;

  size = OBSTACK_OBJECT_SIZE(stack);
  if (size == 0)
    return 0;

  p = OBSTACK_FINISH(stack);
  if (!p)
    return 0;

  OBSTACK_GROW(stack, p, size);
  return OBSTACK_BASE(stack);
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
OBSTACK_DUP_GROW0(struct obstack *stack)
{
  void *p;
  size_t size;

  size = OBSTACK_OBJECT_SIZE(stack);
  if (size == 0)
    return 0;

  OBSTACK_1GROW(stack, '\0');
  p = OBSTACK_FINISH(stack);
  if (!p)
    return 0;

  OBSTACK_GROW(stack, p, size);
  return OBSTACK_BASE(stack);
}


//int OBSTACK_APPEND_GROW(struct obstack *stack, void *data, size_t size);
//int OBSTACK_APPEND_GROW0(struct obstack *stack, void *data, size_t size);

static __inline__ char *
OBSTACK_STR_COPY(struct obstack *stack, const char *s)
{
  return OBSTACK_COPY0(stack, s, strlen(s));
}


static __inline__ char *
OBSTACK_STR_COPY2(struct obstack *stack, const char *s1, const char *s2)
{
  char *ret;
  int l1 = strlen(s1);
  int l2 = strlen(s2);

  ret = OBSTACK_ALLOC(stack, l1 + l2 + 1);
  if (!ret)
    return 0;

  strcpy(ret, s1);
  strcpy(ret + l1, s2);

  return ret;
}


static __inline__ char *
OBSTACK_STR_COPY3(struct obstack *stack,
                  const char *s1, const char *s2, const char *s3)
{
  char *ret;
  int l1 = strlen(s1);
  int l2 = strlen(s2);
  int l3 = strlen(s3);

  ret = OBSTACK_ALLOC(stack, l1 + l2 + l3 + 1);
  if (!ret)
    return 0;

  strcpy(ret, s1);
  strcpy(ret + l1, s2);
  strcpy(ret + l1 + l2, s3);

  return ret;
}


static __inline__ char *
OBSTACK_STR_GROW(struct obstack *stack, const char *s)
{
  return OBSTACK_GROW0(stack, s, strlen(s));
}


static __inline__ char *
OBSTACK_STR_GROW2(struct obstack *stack, const char *s1, const char *s2)
{
  char *base;
  size_t size;
  int l1 = strlen(s1);
  int l2 = strlen(s2);

  size = OBSTACK_OBJECT_SIZE(stack);
  OBSTACK_BLANK(stack, l1 + l2 + 1);
  if (OBSTACK_ERROR())
    return 0;

  base = (char *)OBSTACK_BASE(stack) + size;
  strcpy(base, s1);
  strcpy(base + l1, s2);

  return base;
}


static __inline__ char *
OBSTACK_STR_GROW3(struct obstack *stack,
                  const char *s1, const char *s2, const char *s3)
{
  char *base;
  size_t size;
  int l1 = strlen(s1);
  int l2 = strlen(s2);
  int l3 = strlen(s3);

  size = OBSTACK_OBJECT_SIZE(stack);
  OBSTACK_BLANK(stack, l1 + l2 + l3 + 1);
  if (OBSTACK_ERROR())
    return 0;
  base = (char *)OBSTACK_BASE(stack) + size;
  strcpy(base, s1);
  strcpy(base + l1, s2);
  strcpy(base + l1 + l2, s3);

  return base;
}


#ifdef __cplusplus
END_C_DECLS
#endif

#endif  /* OBSUTIL_H_ */

