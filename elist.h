/* $Id$ */

/* Embedded doubly linked list
 * Copyright (C) 2004  Seong-Kook Shin <cinsky@gmail.com>
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
#ifndef ELIST_H_
#define ELIST_H_

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

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

struct elist {
  struct elist *next, *prev;
};

/*
 * ELIST_ENTRY() -- Returns the pointer to the enclosing struct.
 *
 * ELIST_ENTRY returns the pointer to the enclosing struct if you knew
 * the pointer to the struct elist instance.  PTR is the pointer to
 * the struct elink instance, and TYPE is the type of the enclosing
 * struct, MEMBER is the name of struct elist type.
 *
 * For example:
 *
 * struct foo {
 *   struct elist link;
 *   ...
 * };
 *
 * void bar(struct elist *lnk)
 * {
 *   struct foo *p = ELIST_ENTRY(lnk, struct foo, link);
 *   ...
 * }
 */
#define ELIST_ENTRY(ptr, type, member)   \
        ((type *)((char *)(ptr) - (unsigned long) (&((type *)0)->member)))


#define ELIST_NEXT(x)           ((x).next)
#define ELIST_PREV(x)           ((x).prev)
#define ELIST_INIT(bone)        do { (bone).next = (bone).prev = 0; } while (0)


static __inline__ struct elist *
elist_first(struct elist *node)
{
  if (node) {
    while (node->prev)
      node = node->prev;
  }
  return node;
}


static __inline__ struct elist *
elist_last(struct elist *node)
{
  if (node) {
    while (node->next)
      node = node->next;
  }
  return node;
}


static __inline__ struct elist *
elist_nth(struct elist *node, int n)
{
  while ((n-- > 0) && node)
    node = node->next;
  return node;
}


static __inline__ struct elist *
elist_nth_prev(struct elist *node, int n)
{
  while ((n-- > 0) && node)
    node = node->prev;

  return node;
}


static __inline__ struct elist *
elist_append(struct elist *node, struct elist *elem)
{
  struct elist *p;

  if (node) {
    p = elist_last(node);
    p->next = elem;
    elem->prev = p;
  }
  else
    node = elem;
  return node;
}


static __inline__ struct elist *
elist_prepend(struct elist *node, struct elist *elem)
{
  if (node) {
    if (node->prev) {
      node->prev->next = elem;
      elem->prev = node->prev;
    }
    node->prev = elem;
    elem->next = node;
  }

  return elem;
}


static __inline__ struct elist *
elist_concat(struct elist *node1, struct elist *node2)
{
  struct elist *p;

  if (node2) {
    p = elist_last(node1);
    if (p)
      p->next = node2;
    else
      node1 = node2;
    node2->prev = p;
  }
  return node1;
}


static __inline__ int
elist_length(struct elist *node)
{
  int len = 0;
  while (node) {
    len++;
    node = node->next;
  }
  return len;
}


static __inline__ int
elist_foreach(struct elist *node,
              int (*func)(struct elist *, void *), void *data)
{
  int count = 0;
  while (node) {
    if (func(node, data) < 0)
      break;
    count++;
    node = node->next;
  }
  return count;
}


static __inline__ int
elist_position(struct elist *node, struct elist *elem)
{
  int i = 0;
  while (node) {
    if (node == elem)
      return i;
    i++;
    node = node->next;
  }
  return -1;
}


static __inline__ struct elist *
elist_extract(struct elist *node, struct elist *elem)
{
  if (elem) {
    if (elem->prev)
      elem->prev->next = elem->next;
    if (elem->next)
      elem->next->prev = elem->prev;
    if (node == elem)
      node = node->next;

    elem->next = 0;
    elem->prev = 0;
  }
  return node;
}

/*
 * edque -- deque implementation using elist interface.
 *
 * Suppose you want to make a deque implementation.  You need an
 * opaque type DEQUE, which accepts ELEM as its elements.
 *
 * 1. Add struct elist member (says 'list') into the DEQUE.
 *
 *    struct DEQUE {
 *      struct elink list;
 *      ...
 *    }
 *
 * 2. In the DEQUE create/init function, use edque_init() to
 *    initalize 'link'.
 *
 * 3. Add struct elist member (says 'link') into the ELEM.
 *
 *    struct ELEM {
 *      struct elink link;
 *      ...
 *    };
 *
 * 4. While making your own operations such as push/pop or
 *    insert/remove, use edque_push_back(), edque_push_front(),
 *    edque_pop_back() or edque_pop_front().  These functions require
 *    a pointer to the 'list' member.   For example,
 *
 *    int deque_push_back(struct DEQUE *deque, struct ELEM *elem)
 *    {
 *      edque_push_back(&deque->list, &elem->link);
 *      ...
 *    }
 *
 *    struct ELEM *deque_pop_front(struct DEQUE *deque)
 *    {
 *      struct elist *lp;
 *      struct ELEM *ep;
 *      lp = edque_pop_front(&deque->list);
 *      return ELIST_ENTRY(lp, struct ELEM, link);
 *    }
 */

#define EDQUE_HEAD(ht)  ((ht)->prev)
#define EDQUE_TAIL(ht)  ((ht)->next)

static __inline__ void
edque_init(struct elist *ht)
{
  ELIST_INIT(*ht);
}


static __inline__ void
edque_push_back(struct elist *ht, struct elist *node)
{
  if (EDQUE_TAIL(ht)) {
    node->prev = EDQUE_TAIL(ht);
    node->next = 0;

    EDQUE_TAIL(ht)->next = node;
    EDQUE_TAIL(ht) = node;
  }
  else
    EDQUE_HEAD(ht) = EDQUE_TAIL(ht) = node;
}


static __inline__ void
edque_push_front(struct elist *ht, struct elist *node)
{
  if (EDQUE_HEAD(ht)) {
    node->next = EDQUE_HEAD(ht);
    node->prev = 0;

    EDQUE_HEAD(ht)->prev = node;
    EDQUE_HEAD(ht) = node;
  }
  else
    EDQUE_HEAD(ht) = EDQUE_TAIL(ht) = node;
}


static __inline__ struct elist *
edque_pop_back(struct elist *ht)
{
  struct elist *p = EDQUE_TAIL(ht);

  if (p) {
    if (p->prev) {
      EDQUE_TAIL(ht) = p->prev;

      p->prev->next = 0;
      p->prev = 0;
    }
    else
      EDQUE_HEAD(ht) = EDQUE_TAIL(ht) = 0;
  }

  return p;
}


static __inline__ struct elist *
edque_pop_front(struct elist *ht)
{
  struct elist *p = EDQUE_HEAD(ht);

  if (p) {
    if (p->next) {
      EDQUE_HEAD(ht) = p->next;

      p->next->prev = 0;
      p->next = 0;
    }
    else
      EDQUE_HEAD(ht) = EDQUE_TAIL(ht) = 0;
  }

  return p;
}

#undef EDQUE_HEAD
#undef EDQUE_TAIL

END_C_DECLS

#endif /* ELIST_H_ */
