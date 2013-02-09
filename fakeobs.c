/*
 * GNU obstack emulation, useful for debugging.
 * Copyright (C) 2004  Seong-Kook Shin <cinsky@gmail.com>
 */
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>

#include "fakeobs.h"

#define DEF_MAX_PTRS    32


void (*obstack_alloc_failed_handler)(void);
int fake_alignment_mask;


#define ERROR   do {                                \
                  if (obstack_alloc_failed_handler) \
                    obstack_alloc_failed_handler(); \
                } while (0)


void
obstack_init_(struct obstack *stack, void *(*alloc_func)(size_t))
{
  int i;

  stack->grow = 0;
  stack->grow_size = 0;

  stack->current = -1;

  stack->ptrs = alloc_func(sizeof(void *) * DEF_MAX_PTRS);
  stack->num_ptrs = DEF_MAX_PTRS;
  if (!stack->ptrs) {
    ERROR;
    return;
  }
  for (i = 0; i < DEF_MAX_PTRS; i++)
    stack->ptrs[i] = 0;
}


static int
grow_ptrs(struct obstack *stack, void *(*realloc_func)(void *, size_t))
{
  int n;
  void **p;
  int i;

  n = stack->num_ptrs + DEF_MAX_PTRS;
  p = realloc_func(stack->ptrs, sizeof(void *) * n);
  if (!p) {
    ERROR;
    return -1;
  }
  stack->num_ptrs = n;
  stack->ptrs = p;

  for (i = stack->current + 1; i < stack->num_ptrs; i++) {
    stack->ptrs[i] = 0;
  }

  return 0;
}


void *
obstack_alloc_(struct obstack *stack, int size,
               void *(*realloc_func)(void *, size_t))
{
  void *p = 0;

  if (stack->current + 1 >= stack->num_ptrs)
    if (grow_ptrs(stack, realloc_func) < 0)
      return 0;

  p = realloc_func(0, size);
  if (p) {
    stack->ptrs[++stack->current] = p;
  }
  else
    ERROR;

  return p;
}


void
obstack_free_(struct obstack *stack, void *ptr, void (*free_func)(void *))
{
  int i;

  for (i = stack->current; i >= 0; i--) {
    free_func(stack->ptrs[i]);
    if (stack->ptrs[i] == ptr) {
      stack->current = i - 1;
      stack->ptrs[i] = 0;
      break;
    }
    stack->ptrs[i] = 0;
  }

  if (ptr == NULL) {
    free_func(stack->ptrs);
    stack->ptrs = 0;
  }
}


void *
obstack_copy_(struct obstack *stack, const void *address, int size,
              void *(*realloc_func)(void *, size_t))
{
  void *p = obstack_alloc_(stack, size, realloc_func);
  if (p)
    memcpy(p, address, size);
  return p;
}


void *
obstack_copy0_(struct obstack *stack, const void *address, int size,
               void *(*realloc_func)(void *, size_t))
{
  void *p = obstack_alloc_(stack, size + 1, realloc_func);

  if (p) {
    memcpy(p, address, size);
    *((unsigned char *)p + size) = '\0';
  }
  return p;
}


void
obstack_blank_(struct obstack *stack, int size,
               void *(*realloc_func)(void *, size_t))
{
  void *p;

  if (!stack->grow) {
    if (stack->current + 1 >= stack->num_ptrs) {
      if (grow_ptrs(stack, realloc_func) < 0)
        return;
    }
    stack->current++;
    stack->grow_size = 0;
  }
  stack->grow = 1;
  p = stack->ptrs[stack->current];
  p = realloc_func(p, stack->grow_size + size + 1);
  if (p) {
    stack->grow_size += size;
    stack->ptrs[stack->current] = p;
  }
  else
    ERROR;
}


void
obstack_grow_(struct obstack *stack, const void *address, int size,
               void *(*realloc_func)(void *, size_t))
{
  void *p;

  if (!stack->grow) {
    if (stack->current + 1 >= stack->num_ptrs) {
      if (grow_ptrs(stack, realloc_func) < 0)
        return;
    }
    stack->current++;
    stack->grow_size = 0;
  }
  stack->grow = 1;
  p = stack->ptrs[stack->current];
  p = realloc_func(p, stack->grow_size + size);
  if (p) {
    stack->grow_size += size;
    stack->ptrs[stack->current] = p;
    memcpy((unsigned char *)p + stack->grow_size - size, address, size);
  }
  else
    ERROR;
}


void
obstack_grow0_(struct obstack *stack, const void *address, int size,
               void *(*realloc_func)(void *, size_t))
{
  void *p;

  if (!stack->grow) {
    if (stack->current + 1 >= stack->num_ptrs) {
      if (grow_ptrs(stack, realloc_func) < 0)
        return;
    }
    stack->current++;
    stack->grow_size = 0;
  }
  stack->grow = 1;
  p = stack->ptrs[stack->current];
  p = realloc_func(p, stack->grow_size + size + 1);
  if (p) {
    stack->grow_size += size + 1;
    stack->ptrs[stack->current] = p;
    memcpy((unsigned char *)p + stack->grow_size - size - 1, address, size);
    *((unsigned char *)p + stack->grow_size - 1) = '\0';
  }
  else
    ERROR;
}


void
obstack_1grow_(struct obstack *stack, char data,
               void *(*realloc_func)(void *, size_t))
{
  void *p;

  if (!stack->grow) {
    if (stack->current - 1 >= stack->num_ptrs) {
      if (grow_ptrs(stack, realloc_func) < 0)
        return;
    }
    stack->current++;
    stack->grow_size = 0;
  }
  stack->grow = 1;
  p = stack->ptrs[stack->current];
  p = realloc_func(p, stack->grow_size + 1);
  if (p) {
    stack->grow_size += 1;
    stack->ptrs[stack->current] = p;
    *((unsigned char *)p + stack->grow_size - 1) = data;
  }
  else
    ERROR;
}


void
obstack_ptr_grow(struct obstack *s, void *data)
{
  void *val = data;
  obstack_grow(s, &val, sizeof(val));
}

void
obstack_int_grow(struct obstack *s, int data)
{
  int val = data;
  obstack_grow(s, &val, sizeof(val));
}


void
obstack_ptr_grow_fast(struct obstack *stack, void *data)
{
  abort();
}


void
obstack_int_grow_fast(struct obstack *stack, int data)
{
  abort();
}


void
obstack_blank_fast(struct obstack *stack, int size)
{
  abort();
}


void
obstack_1grow_fast(struct obstack *stack, char c)
{
  abort();
}


static int
ismapped(void *ptr)
{
  unsigned char vec;

  if (mincore(ptr, 1, &vec) == 0)
    return 1;
  else if (errno == ENOMEM)
    return 0;
  abort();
}


#ifdef TEST_FAKEOBS
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int
main(void)
{
  struct obstack pool;
  int i;
  char *mark;
  char *p;

  srandom(time(NULL));
  obstack_init(&pool);

  for (i = 0; i < 32; i++) {
    p = obstack_alloc(&pool, random() % 4096);

    if (i == 10)
      mark = p;
  }
  obstack_free(&pool, mark);

  for (i = 0; i < 10; i++) {
    obstack_blank(&pool, random() % 4096);
  }
  obstack_finish(&pool);
  obstack_free(&pool, NULL);
  return 0;
}

#endif  /* TEST_FAKEOBS */
