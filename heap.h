#ifndef HEAP_H__
#define HEAP_H__

/*
 * Copyright (C) 2014  Seong-Kook Shin <cinsky@gmail.com>
 * DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 * Version 2, December 2004
 *
 * Copyright (C) 2014 Seong-Kook Shin <cinsky@gmail.com>
 *
 * Everyone is permitted to copy and distribute verbatim or modified
 * copies of this license document, and changing it is allowed as long
 * as the name is changed.
 *
 *            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
 *
 *  0. You just DO WHAT THE FUCK YOU WANT TO.
 *
 * This program is free software. It comes without any warranty, to the
 * extent permitted by applicable law. You can redistribute it and/or
 * modify it under the terms of the Do What The Fuck You Want To Public
 * License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

#define HEAP_DECL_TYPE(elem_type, name)         \
  typedef struct {                              \
    elem_type **body;                           \
    elem_type *t_swap;                          \
    elem_type *t_ret;                           \
    int end;                                    \
    size_t size;                                \
  } name

//#define HEAP_INITIALIZER        { 0, 0, 0, 0, 64 }
#define HEAP_INITIALIZER(size)        { 0, 0, 0, 0, (size) / 2 }

#define HEAP_PARENT(x)  (((x) - 1) / 2)
#define HEAP_LCHILD(x)  ((x) * 2 + 1)
#define HEAP_RCHILD(x)  ((x) * 2 + 2)

#define HEAP_FREE(heap)                         \
        do {                                    \
          if ((heap)->body)                     \
            free((heap)->body);                 \
          (heap)->body = 0;                     \
          (heap)->end = 0;                      \
          (heap)->size = 0;                     \
        } while (0)

#define HEAP_MAKE_ROOM_(heap, result)                           \
  do {                                                          \
    void *p;                                                    \
    if (!(heap)->body || (heap)->end >= (heap)->size) {         \
      p = realloc((heap)->body,                                 \
                  (heap)->size * 2  * sizeof((heap)->body[0])); \
      if (p) {                                                  \
        (heap)->body = p;                                       \
        (heap)->size = (heap)->size * 2;                        \
        (result) = 1;                                           \
      }                                                         \
      else                                                      \
        (result) = 0;                                           \
    }                                                           \
  } while (0)

#define HEAP_BALANCE_(type, heap, index)                                \
  do {                                                                  \
    int i = index;                                                      \
    while (i > 0) {                                                     \
      int parent = HEAP_PARENT(i);                                      \
      __builtin_prefetch((heap)->body[parent]);                          \
      __builtin_prefetch((heap)->body[i]);                              \
      __builtin_prefetch((heap)->body[HEAP_PARENT(parent)]);            \
      if (type##_cmp((heap)->body[parent], (heap)->body[i]) < 0) {      \
        (heap)->t_swap = (heap)->body[parent];                          \
        (heap)->body[parent] = (heap)->body[i];                         \
        (heap)->body[i] = (heap)->t_swap;                               \
        i = parent;                                                     \
      }                                                                 \
      else break;                                                       \
    }                                                                   \
  } while (0)

#define HEAP_INSERT_(type, heap, nodep, result)         \
    do {                                                \
      int success;                                      \
      HEAP_MAKE_ROOM_(heap, success);                   \
      if (success) {                                    \
        (heap)->body[(heap)->end++] = nodep;            \
        HEAP_BALANCE_(type, heap, (heap)->end - 1);     \
      }                                                 \
      (result) = !success;                              \
    } while (0)

#define HEAP_POP_(type, heap, result)                                   \
    do {                                                                \
      (heap)->t_ret = 0;                                                \
      if ((heap)->end > 0) {                                            \
        int c = 0;                                                      \
        (heap)->t_ret = (heap)->body[0];                                \
        (heap)->body[0] = (heap)->body[(heap)->end - 1];                \
        (heap)->end--;                                                  \
        (heap)->body[(heap)->end] = 0;                                  \
                                                                        \
        while (c < (heap)->end) {                                       \
          int child = HEAP_LCHILD(c);                                   \
                                                                        \
          if (child >= (heap)->end)                                     \
            break;                                                      \
                                                                        \
          int rchild = HEAP_RCHILD(c);                                  \
          __builtin_prefetch((heap)->body[child]);                      \
          __builtin_prefetch((heap)->body[c]);                          \
          __builtin_prefetch((heap)->body[rchild]);                     \
          if (rchild < (heap)->end &&                                   \
              type##_cmp((heap)->body[child], (heap)->body[rchild]) < 0) \
            child = rchild;                                             \
                                                                        \
          if (type##_cmp((heap)->body[c], (heap)->body[child]) < 0) {   \
            (heap)->t_swap = (heap)->body[c];                           \
            (heap)->body[c] = (heap)->body[child];                      \
            (heap)->body[child] = (heap)->t_swap;                       \
          }                                                             \
          c = child;                                                    \
        }                                                               \
      }                                                                 \
      (result) = (heap)->t_ret;                                         \
    } while (0)

#ifdef __GNUC__
#define HEAP_INSERT(type, heap, nodep)          \
    ({                                          \
      int result;                               \
      HEAP_INSERT_(type, heap, nodep, result);  \
      result; })


#define HEAP_POP(type, heap)                    \
      ({ typeof((heap)->body[0]) result;        \
         HEAP_POP_(type, heap, result);         \
         result; })
#endif  /* __GNUC__ */

#endif  /* HEAP_H__ */
