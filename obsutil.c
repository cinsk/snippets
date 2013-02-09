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

#include <obsutil.h>

int obstack_errno_;

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


#ifndef FAKEOBJS
size_t
obstack_capacity(struct obstack *stack)
{
  struct _obstack_chunk *chunk = stack->chunk;
  size_t cap = 0;

  while (chunk) {
    cap += (unsigned char *)chunk->limit - (unsigned char *)chunk;
    chunk = chunk->prev;
  }

  return cap;
}
#else
size_t
obstack_capacity(struct obstack *stack)
{
  return 0;
}
#endif  /* FAKEOBJS */

#ifndef NDEBUG
#include <stdio.h>
/*
 * These two functions are provided for debugging obstack.
 * You can call these functions inside GDB using 'print' command.
 */
void
obstack_dump(struct obstack *stack)
{
#ifndef FAKEOBJS
  struct _obstack_chunk *chunk;
  int i = 0;

  printf("== obstack dump--\n");
  printf("== obstack 0x%08x\n", (unsigned)stack);
  printf("==   chunk_size: %ld\n", stack->chunk_size);
  printf("==   chunk:          [0x%08x]\n", (unsigned)stack->chunk);
  printf("==   next_free:      [0x%08x]\n", (unsigned)stack->next_free);
  printf("==   chunk_limit:    [0x%08x]\n", (unsigned)stack->chunk_limit);
  //printf("==   temp: %ld\n", (unsigned long)stack->temp);
  printf("==   alignment_mask: 0x%lx\n", (unsigned long)stack->alignment_mask);
  printf("==   use_extra_arg:      %u\n", stack->use_extra_arg);
  printf("==   maybe_empty_object: %u\n", stack->maybe_empty_object);
  printf("==   alloc_failed:       %u\n", stack->alloc_failed);

  chunk = stack->chunk;
  while (chunk) {
    i++;
    printf("==\n");
    printf("==   %dnd chunk: [0x%08x]\n", i, (unsigned)chunk);
    printf("==                     prev: [0x%08x]\n", (unsigned)chunk->prev);
    printf("==                    limit: [0x%08x]\n", (unsigned)chunk->limit);
    printf("==        size(limit-chunk): %ld\n",
           (unsigned long)((unsigned char *)chunk->limit -
                           (unsigned char *)chunk));

    chunk = chunk->prev;
  }
#endif  /* FAKEOBJS */
}


#ifndef FAKEOBJS
int
obstack_belong(struct obstack *stack, void *ptr, size_t size)
{
  struct _obstack_chunk *chunk;

  for (chunk = stack->chunk;
       chunk != NULL;
       chunk = chunk->prev) {
    if ((unsigned char *)ptr < (unsigned char *)chunk ||
        (unsigned char *)ptr >= (unsigned char *)(chunk->limit))
      continue;

    if (size > 0) {
      if ((unsigned char *)ptr + size < (unsigned char *)chunk->limit)
        return 1;
    }
    return 1;
  }
  return 0;
}
#else
int
obstack_belong(struct obstack *stack, void *ptr, size_t size)
{
  return -1;
}
#endif  /* FAKEOBJS */
#endif  /* NDEBUG */


#ifdef TEST_OBSUTIL

int
main(int argc, char *argv[])
{
  struct obstack stack_;
  struct obstack *stack = &stack_;
  void *p, *q, *r;

  if (OBSTACK_INIT(stack) < 0) {
    fprintf(stderr, "OBSTACK_INIT() failed");
    return 1;
  }

  p = OBSTACK_ALLOC(stack, 100);
  q = OBSTACK_ALLOC(stack, 100);
  r = OBSTACK_ALLOC(stack, 8196);
  OBSTACK_ALLOC(stack, 100);
  OBSTACK_ALLOC(stack, 100);
  OBSTACK_ALLOC(stack, 100);
  OBSTACK_FREE(stack, r);
  OBSTACK_FREE(stack, p);

  return 0;
}

#endif  /* TEST_OBSUTIL */



