/* $Id$ */

/*
 * Embedded doubly linked list
 * Copyright (C) 2004  Seong-Kook Shin <cinsky@gmail.com>
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

#define ELIST_ENTRY(ptr, type, member)   \
        ((type *)((char *)(ptr) - (unsigned long) (&((type *)0)->member)))


#define ELIST_NEXT(x)           ((x).next)
#define ELIST_PREV(x)           ((x).prev)
#define ELIST_INIT(bone)        do { bone.next = bone.prev = 0; } while (0)


static __inline__ struct elist *
elist_first(struct elist *lst)
{
  if (lst) {
    while (lst->prev)
      lst = lst->prev;
  }
  return lst;
}


static __inline__ struct elist *
elist_last(struct elist *lst)
{
  if (lst) {
    while (lst->next)
      lst = lst->next;
  }
  return lst;
}


static __inline__ struct elist *
elist_nth(struct elist *lst, int n)
{
  while ((n-- > 0) && lst)
    lst = lst->next;
  return lst;
}


static __inline__ struct elist *
elist_nth_prev(struct elist *lst, int n)
{
  while ((n-- > 0) && lst)
    lst = lst->prev;

  return lst;
}


static __inline__ struct elist *
elist_append(struct elist *lst, struct elist *elem)
{
  struct elist *p;

  if (lst) {
    p = elist_last(lst);
    p->next = elem;
    elem->prev = p;
  }
  else
    lst = elem;
  return lst;
}


static __inline__ struct elist *
elist_prepend(struct elist *lst, struct elist *elem)
{
  if (lst) {
    if (lst->prev) {
      lst->prev->next = elem;
      elem->prev = lst->prev;
    }
    lst->prev = elem;
    elem->next = lst;
  }

  return elem;
}


static __inline__ struct elist *
elist_concat(struct elist *lst1, struct elist *lst2)
{
  struct elist *p;

  if (lst2) {
    p = elist_last(lst1);
    if (p)
      p->next = lst2;
    else
      lst1 = lst2;
    lst2->prev = p;
  }
  return lst1;
}


static __inline__ int
elist_length(struct elist *lst)
{
  int len = 0;
  while (lst) {
    len++;
    lst = lst->next;
  }
  return len;
}


static __inline__ int
elist_foreach(struct elist *lst,
              int (*func)(struct elist *, void *), void *data)
{
  int count = 0;
  while (lst) {
    if (func(lst, data) < 0)
      break;
    count++;
    lst = lst->next;
  }
  return count;
}


static __inline__ int
elist_position(struct elist *lst, struct elist *elem)
{
  int i = 0;
  while (lst) {
    if (lst == elem)
      return i;
    i++;
    lst = lst->next;
  }
  return -1;
}


static __inline__ struct elist *
elist_extract(struct elist *lst, struct elist *elem)
{
  if (elem) {
    if (elem->prev)
      elem->prev->next = elem->next;
    if (elem->next)
      elem->next->prev = elem->prev;
    if (lst == elem)
      lst = lst->next;

    elem->next = 0;
    elem->prev = 0;
  }
  return lst;
}

END_C_DECLS

#endif /* ELIST_H_ */
