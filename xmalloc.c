/* $Id$ */
/*
 * malloc debugger
 * Copyright (C) 2004  Seong-Kook Shin <cinsky@gmail.com>
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xmalloc.h>

static long xmem_limit = -1;
static int malloc_called;
static int calloc_called;
static int realloc_called;

static int free_called;
static size_t xmem_cur_size;
static size_t xmem_max_size;
static FILE *xmem_error_stream = stderr;
static void (*xalloc_failed_handler)(void) = abort;

#define XMEM_HEADER ((0x2B2B56 << 16) + 0x1F6781)
#define XMEM_FOOTER 0xdeadbeef

struct xmemheader {
  size_t size;
  unsigned int redzone;
};

struct xmemfooter {
  unsigned int redzone[2];
};


static __inline__ size_t
ptr_size(size_t size)
{
#ifdef XMEM_STAT
  size += sizeof(struct xmemheader);
#endif

#ifdef XMEM_REDZONE
  size += sizeof(struct xmemfooter);
#endif

  return size;
}


static __inline__ void *
set_ptr(void *ptr, size_t size)
{
#ifdef XMEM_STAT
  struct xmemheader head;
  head.redzone = XMEM_HEADER;
  head.size = size;
  memcpy(ptr, &head, sizeof(head));
#endif

#ifdef XMEM_REDZONE
  {
    static struct xmemfooter foot = { { XMEM_FOOTER, XMEM_FOOTER } };
    memcpy((char *)ptr + sizeof(head) + size, &foot, sizeof(foot));
  }
#endif

#ifdef XMEM_STAT
  return (char *)ptr + sizeof(head);
#else
  return ptr;


#endif /* XMEM_STAT */
}


static __inline__ void *
get_ptr(void *ptr)
{
#ifdef XMEM_STAT
  return (char *)ptr - sizeof(struct xmemheader);
#else
  return (char *)ptr;
#endif
}


static __inline__ int
add_stat(size_t size)
{
  if (xmem_limit > 0 && xmem_cur_size + size > xmem_limit)
    return -1;
  xmem_cur_size += size;
  if (xmem_max_size < xmem_cur_size)
    xmem_max_size = xmem_cur_size;
  return 0;
}


void *
xmalloc(size_t size)
{
  void *ptr;

  if (size == 0)
    size = 1;

  ptr = malloc(ptr_size(size));
  if (!ptr || add_stat(size) < 0)
    goto err;
  malloc_called++;
  return set_ptr(ptr, size);

 err:
  if (xalloc_failed_handler)
    xalloc_failed_handler();
  return 0;
}


void *
xcalloc(size_t nmemb, size_t size)
{
  void *ptr;

  size = size * nmemb;
  if (size == 0)
    size = 1;
  ptr = malloc(ptr_size(size));
  if (!ptr || add_stat(size) < 0)
    goto err;
  calloc_called++;
  return set_ptr(ptr, size);

 err:
  if (xalloc_failed_handler)
    xalloc_failed_handler();
  return 0;
}


void *
xrealloc(void *ptr, size_t size)
{
  if (size == 0)
    size = 1;
  ptr = get_ptr(ptr);
  ptr = realloc(ptr, ptr_size(size));
  if (!ptr || add_stat(size) < 0)
    goto err;
  realloc_called++;
  return set_ptr(ptr, size);

 err:
  if (xalloc_failed_handler)
    xalloc_failed_handler();
  return 0;
}


static __inline__ int
redzone_check(void *ptr, size_t size)
{
#ifdef XMEM_REDZONE
  int i = 0;
  struct xmemfooter *p = (struct xmemfooter *)((char *)ptr +
                                               sizeof(struct xmemheader) +
                                               size);

  while (i < sizeof(p->redzone) / sizeof(int)) {
    if (p->redzone[i] != XMEM_FOOTER)
      return -1;
    i++;
  }
#endif /* XMEM_REDZONE */
  return 0;
}


void
xfree(void *ptr)
{
#ifdef XMEM_STAT
  {
    struct xmemheader *p;
    p = (struct xmemheader *)get_ptr(ptr);
    if (p->redzone != XMEM_HEADER) {
      /* Invalid usage of xfree(), PTR wasn't generated by xmalloc() */
      if (xmem_error_stream)
        fprintf(xmem_error_stream,
                "error: xfree() failed: redzone is not valid\n");
      return;
    }

    xmem_cur_size -= p->size;
    if (redzone_check(p, p->size) < 0 && xalloc_failed_handler)
      xalloc_failed_handler();
  }
#endif

  free_called++;
  free(get_ptr(ptr));
}


long
xmemopt(int option, ...)
{
  va_list ap;

  switch (option) {
#ifdef XMEM_STAT
  case X_SETLIM:
    va_start(ap, option);
    xmem_limit = va_arg(ap, long);
    va_end(ap);
    break;
  case X_GETLIM:
    return xmem_limit;
#endif /* XMEM_STAT */
  case X_SETES:
    va_start(ap, option);
    xmem_error_stream = va_arg(ap, FILE *);
    va_end(ap);
    break;
  case X_GETES:
    return (long)xmem_error_stream;
  case X_SETFH:
    va_start(ap, option);
    xalloc_failed_handler = va_arg(ap, void (*)(void));
    va_end(ap);
    break;
  case X_GETFH:
    return (long)xalloc_failed_handler;

  default:
    return -1;
  }
  return 0;
}

void
xmemstat(memstat_t *ms)
{
  ms->malloc_called = malloc_called;
  ms->calloc_called = calloc_called;
  ms->realloc_called = realloc_called;
  ms->free_called = free_called;

  ms->cur_size = xmem_cur_size;
  ms->max_size = xmem_max_size;
  ms->limit = xmem_limit;
}


#ifdef TEST_XMALLOC
#include <stdio.h>

int
main(void)
{
  memstat_t ms;
  char *vec[80];
  int i, j;

  xmemopt(X_SETLIM, 10000);
  xmemopt(X_SETFH, abort);

  for (i = 0; i < 80; i++) {
    int size = rand() % 1000;
    printf("xmalloc(%d) called..\n", size);
    vec[i] = xmalloc(size);
    for (j = 0; j < size; j++) {
      vec[i][j] = '*';
    }
  }

  for (i = 0; i < 60; i++) {
    int size = rand() % 1000;
    printf("xrealloc(0x%08X, %d) called..\n", (int)vec[i], size);
    vec[i] = xrealloc(vec[i], size);
    for (j = 0; j < size; j++) {
      vec[i][j] = '*';
    }
  }

  for (i = 0; i < 80; i++) {
    xfree(vec[i]);
  }

  xmemstat(&ms);
  return 0;
}
#endif /* TEST_XMALLOC */
