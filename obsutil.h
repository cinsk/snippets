/* $Id$ */
/*
 * GNU obstack wrapper with some utilities
 * Copyright (C) 2003, 2004  Seong-Kook Shin <cinsky@gmail.com>
 */
#ifndef OBSUTIL_H_
#define OBSUTIL_H_

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <obstack.h>

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
OBSTACK_COPY(struct obstack *stack, void *address, int size)
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
OBSTACK_PTR_GROW(struct obstack *stack, void *data)
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


struct os_slist_ {
  void *data;
  struct os_slist_ *next;
};
typedef struct os_slist_ os_slist_t;

#define OS_SLIST_DATA(list)     ((list)->data)
#define OS_SLIST_NEXT(list)     ((list)->next)

struct os_slist_man_ {
  os_slist_t *grave;
  struct obstack *stack;        /* backward reference */

  int num_zombies;
  int num_nodes;
};
typedef struct os_slist_man_ os_slist_man_t;


extern os_slist_man_t *os_slist_init(struct obstack *stack);
extern os_slist_t *os_slist_node_new(os_slist_man_t *manager, void *data);
extern os_slist_t *os_slist_new_v(os_slist_man_t *manager, int nelem, ...);


static __inline__ void *
os_slist_node_delete_(os_slist_man_t *manager, os_slist_t *node)
{
  void *ret = node->data;

  node->next = manager->grave;
  manager->grave = node;
  manager->num_zombies++;
  return ret;
}


static __inline__ void
os_slist_delete(os_slist_man_t *manager, os_slist_t *list)
{
  os_slist_t *ptr = list;

  while (ptr) {
    os_slist_t *tmp = ptr->next;
    os_slist_node_delete_(manager, ptr);
    ptr = tmp;
  }
}


static __inline__ os_slist_t *
os_slist_append(os_slist_man_t *manager, os_slist_t *dest, os_slist_t *src)
{
  os_slist_t *last, *tmp;

  for (tmp = dest; tmp != NULL; tmp = tmp->next)
    last = tmp;

  last->next = src;
  return dest;
}


static __inline__ os_slist_t *
os_slist_prepend(os_slist_man_t *manager, os_slist_t *dest, os_slist_t *src)
{
  os_slist_t *last, *tmp;

  for (tmp = src; tmp != NULL; tmp = tmp->next)
    last = tmp;

  last->next = dest;
  return src;
}


static __inline__ os_slist_t *
os_slist_next(os_slist_man_t *manager, os_slist_t *list)
{
  return list->next;
}


static __inline__ os_slist_t *
os_slist_find_first(os_slist_man_t *manager,
                    os_slist_t *list, void *data)
{
  os_slist_t *ptr;
  for (ptr = list; ptr != NULL; ptr = ptr->next)
    if (ptr->data == data)
      return ptr;
  return 0;
}


static __inline__ os_slist_t *
os_slist_find_nth(os_slist_man_t *manager,
                  os_slist_t *list, void *data, int nth)
{
  os_slist_t *ptr;
  int match = 0;

  for (ptr = list; ptr != 0; ptr = ptr->next) {
    if (ptr->data == data) {
      if (match == nth)
        return ptr;
      match++;
    }
  }
  return 0;
}


static __inline__ os_slist_t *
os_slist_find_last(os_slist_man_t *manager,
                   os_slist_t *list, void *data)
{
  os_slist_t *ptr, *ptr2 = 0;

  for (ptr = list; ptr != NULL; ptr = ptr->next) {
    if (ptr->data == data) {
      ptr2 = ptr;
    }
  }
  return ptr2;
}


#ifdef __cplusplus
END_C_DECLS
#endif

#endif  /* OBSUTIL_H_ */

