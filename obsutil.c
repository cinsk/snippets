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

#include <stdarg.h>
#include <stdio.h>
#include "obsutil.h"

#ifdef _PTHREAD
#include <pthread.h>

#define ERRBUF_LEN      256

pthread_once_t obsutil_once = PTHREAD_ONCE_INIT;

pthread_key_t obsutil_key_errno;
pthread_key_t obsutil_key_errbuf;

static void obsutil_thread_init_once(void);

char *
obsutil_strerror(int errnum)
{
  char *buf = pthread_getspecific(obsutil_key_errbuf);
  if (!buf) {
    buf = malloc(ERRBUF_LEN);
    if (!buf)
      return NULL;
    pthread_setspecific(obsutil_key_errbuf, buf);
    //buf = pthread_getspecific(obsutil_key_errbuf);
  }

  strerror_r(errnum, buf, ERRBUF_LEN);
  return buf;
}


void
obsutil_thread_init(void)
{
  int ret = pthread_once(&obsutil_once, obsutil_thread_init_once);

  if (ret) {
    fprintf(stderr, "error: obsutil_thread_init() failed: %s\n",
            obsutil_strerror(ret));
    abort();
  }
}


static void
obsutil_thread_init_once(void)
{
  char errbuf[ERRBUF_LEN];

  int ret = pthread_key_create(&obsutil_key_errno, NULL);
  if (ret) {
    strerror_r(ret, errbuf, ERRBUF_LEN);
    fprintf(stderr, "%s:%d: pthread_key_create() failed: %s\n",
            __FILE__, __LINE__, errbuf);
    abort();
  }

  ret = pthread_key_create(&obsutil_key_errbuf, NULL);
  if (ret) {
    strerror_r(ret, errbuf, ERRBUF_LEN);
    fprintf(stderr, "%s:%d: pthread_key_create() failed: %s\n",
            __FILE__, __LINE__, errbuf);
    abort();
  }
}


void
obsutil_set_errno(int errno)
{
  char errbuf[ERRBUF_LEN] = { 0, };

  int *p = obsutil_errno_();
  *p = errno;
}


int *
obsutil_errno_(void)
{
  int ret;
  int *p = pthread_getspecific(obsutil_key_errno);

  if (!p) {
    p = malloc(sizeof(int));
    if (!p) {
      fprintf(stderr, "%s:%d: malloc() failed: %s\n",
              __FILE__, __LINE__,
              obsutil_strerror(errno));
      abort();
    }
    ret = pthread_setspecific(obsutil_key_errno, p);
    if (ret) {
      fprintf(stderr, "%s:%d: pthread_setspecific() failed: %s\n",
              __FILE__, __LINE__,
              obsutil_strerror(ret));
      abort();
    }
  }
  return p;
}

#else  /* _PTHREAD */
int obs_errno = 0;
#endif  /* _PTHREAD */

int obsutil_errno;

void
obsutil_alloc_failed_handler(void)
{
  if (errno != 0)
    obsutil_errno = errno;
  else
    obsutil_errno = 1;
}

obstack_alloc_failed_handler_t
obsutil_init(void)
{
  obstack_alloc_failed_handler_t handler = obstack_alloc_failed_handler;
  obstack_alloc_failed_handler = obsutil_alloc_failed_handler;
  return handler;
}


char *
obs_format(struct obstack *stack, const char *format, ...)
{
  va_list ap;
  int len;
  char *p;

  assert(stack != NULL);
  assert(obs_object_size(stack) == 0);

  va_start(ap, format);
  len = vsnprintf(NULL, 0, format, ap);
  va_end(ap);

  p = obs_alloc(stack, len + 1);
  if (p) {
    va_start(ap, format);
    vsprintf(p, format, ap);
    va_end(ap);
  }
  return p;
}


char *
obs_grow_format(struct obstack *stack, const char *format, ...)
{
  va_list ap;
  int len, pos;
  char *p = NULL;

  assert(stack != NULL);

  va_start(ap, format);
  len = vsnprintf(NULL, 0, format, ap);
  va_end(ap);

  pos = obs_object_size(stack);
  if (obs_blank(stack, len + 1) == 0) {
    p = obs_base(stack) + pos;
    va_start(ap, format);
    vsprintf(p, format, ap);
    va_end(ap);
  }
  return p;
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
  printf("== obstack 0x%08lx\n", (unsigned long)stack);
  printf("==   chunk_size: %ld\n", stack->chunk_size);
  printf("==   chunk:          [0x%08lx]\n", (unsigned long)stack->chunk);
  printf("==   next_free:      [0x%08lx]\n", (unsigned long)stack->next_free);
  printf("==   chunk_limit:    [0x%08lx]\n", (unsigned long)stack->chunk_limit);
  //printf("==   temp: %ld\n", (unsigned long)stack->temp);
  printf("==   alignment_mask: 0x%lx\n", (unsigned long)stack->alignment_mask);
  printf("==   use_extra_arg:      %u\n", stack->use_extra_arg);
  printf("==   maybe_empty_object: %u\n", stack->maybe_empty_object);
  printf("==   alloc_failed:       %u\n", stack->alloc_failed);

  chunk = stack->chunk;
  while (chunk) {
    i++;
    printf("==\n");
    printf("==   %dnd chunk: [0x%08lx]\n", i, (unsigned long)chunk);
    printf("==                     prev: [0x%08lx]\n", (unsigned long)chunk->prev);
    printf("==                    limit: [0x%08lx]\n", (unsigned long)chunk->limit);
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

void *
child_thread(void *arg)
{
  obsutil_errno = 0xdeadbeef;
  printf("obsutil_errno = %x\n", obsutil_errno);
  sleep(3);
  return NULL;
}

int
main(int argc, char *argv[])
{
  struct obstack stack_;
  struct obstack *stack = &stack_;
  void *p, *q, *r;

  obsutil_init();
#ifdef _PTHREAD
  {
    pthread_t child;

    obsutil_thread_init();


    obsutil_errno = 0;
    printf("obsutil_errno = %x\n", obsutil_errno);
    pthread_create(&child, NULL, child_thread, NULL);

    pthread_join(child, NULL);
    printf("obsutil_errno = %x\n", obsutil_errno);
  }
#endif

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
