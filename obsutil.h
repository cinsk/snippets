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

#ifdef __cplusplus
END_C_DECLS
#endif

#endif  /* OBSUTIL_H_ */

