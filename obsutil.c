/* $Id$ */
/*
 * GNU obstack wrapper with some utilities
 * Copyright (C) 2003, 2004  Seong-Kook Shin <cinsky@gmail.com>
 */
#include <assert.h>
#include <stdarg.h>

#include <obsutil.h>


int obstack_errno_ = 0;


void
obsutil_alloc_failed_handler(void)
{
  obstack_errno_ = 1;
}


obstack_alloc_failed_handler_t
obsutil_init(void)
{
  obstack_alloc_failed_handler_t handler = obstack_alloc_failed_handler;
  obstack_alloc_failed_handler = obsutil_alloc_failed_handler;
  return handler;
}


os_slist_man_t *
os_slist_init(struct obstack *stack)
{
  os_slist_man_t *manager;
  assert(OBSTACK_OBJECT_SIZE(stack) == 0);

  manager = OBSTACK_ALLOC(stack, sizeof(*manager));
  if (obstack_errno_)
    return 0;
  manager->grave = 0;
  manager->stack = stack;

  manager->num_zombies = 0;
  manager->num_nodes = 0;

  return manager;
}


os_slist_t *
os_slist_node_new(os_slist_man_t *manager, void *data)
{
  os_slist_t *node;
  assert(OBSTACK_OBJECT_SIZE(manager->stack) == 0);

  if (manager->grave) {
    node = manager->grave;
    manager->grave = node->next;
    manager->num_zombies--;
  }
  else {
    node = OBSTACK_ALLOC(manager->stack, sizeof(*node));
    if (obstack_errno_)
      return 0;
    manager->num_nodes++;
  }
  node->next = 0;
  node->data = data;

  return node;
}


os_slist_t *
os_slist_new_v(os_slist_man_t *manager, int nelem, ...)
{
  va_list argptr;
  int i;
  os_slist_t *list = 0, *last, *ptr;

  if (nelem < 1)
    return 0;

  va_start(argptr, nelem);
  list = os_slist_node_new(manager, va_arg(argptr, void *));
  last = list;

  for (i = 1; i < nelem; i++) {
    ptr = os_slist_node_new(manager, va_arg(argptr, void *));
    if (!ptr) {
      os_slist_delete(manager, list);
      va_end(argptr);
      return 0;
    }
    last->next = ptr;
    last = ptr;
  }

  va_end(argptr);
  return list;
}


#ifdef TEST_OBSUTIL

#include <error.h>

int
main(void)
{
  struct obstack stack_;
  struct obstack *stack = &stack_;
  os_slist_man_t *manager;
  os_slist_t *list;

  obsutil_init();

  if (OBSTACK_INIT(stack) < 0)
    error(1, 0, "obstack_init() failed");

  manager = os_slist_init(stack);
  if (!manager)
    error(1, 0, "os_slist_init() failed");

  list = os_slist_new_v(manager, 4, NULL, NULL, NULL, NULL);


  return 0;
}
#endif  /* TEST_OBSUTIL */
