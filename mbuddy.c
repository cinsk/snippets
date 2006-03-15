/* $Id$ */
/* mbuddy: malloc debugging buddy
 * Copyright (C) 2006  Seong-Kook Shin <cinsky@gmail.com>
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

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/time.h>
#include <pthread.h>

#include <malloc.h>
#include <execinfo.h>

#define MB_LOGNAME      "mb.out"
#define MAX_BACKTRACE   32

pthread_mutex_t mb_mutex = PTHREAD_MUTEX_INITIALIZER;

size_t mb_alloc_peak = 0;
size_t mb_allocated = 0;

int malloc_called = 0;
int realloc_called = 0;
int realloc_malloc = 0;
int realloc_free = 0;
int free_called = 0;

FILE *mb_stream;

#define LOCK()       pthread_mutex_lock(&mb_mutex)
#define UNLOCK()     pthread_mutex_unlock(&mb_mutex)


#define ALIGN_OFFSET    4
#define ALIGN_BITS      2

#define ALIGN_NEXT(x)   ((x + ALIGN_OFFSET) >> ALIGN_BITS << ALIGN_BITS)

struct mb_head {
  size_t size;
  time_t created;
  time_t modified;
};

/*
 * MB_LOGNAME
 * MB_LIMIT
 * MB_NLIMIT
 * MB_BACKTRACE
 */

#define HEADER_SIZE     ALIGN_NEXT(sizeof(struct mb_head))
#define HEADER(p)       ((struct mb_head *)((unsigned char *)p - HEADER_SIZE))
#define SET_HEAD(p, s)  do { if (p) ((struct mb_head *)p)->size = (s); } while (0)

#define RESTORE_HOOK()  do { \
                          __malloc_hook = old_malloc_hook; \
                          __realloc_hook = old_realloc_hook; \
                          __free_hook = old_free_hook; \
                        } while (0)

#define SAVE_HOOK()     do { \
                          __malloc_hook = mb_malloc; \
                          __realloc_hook = mb_realloc; \
                          __free_hook = mb_free; \
                        } while (0)

#define SET_PEAK()      do { \
                          if (mb_allocated > mb_alloc_peak) \
                            mb_alloc_peak = mb_allocated; \
                        } while (0)

/*
 * MBUDDY_LIMIT="1024k"
 * MBUDDY_NLIMIT="12"
 * MBUDDY_
 */

void *mb_malloc(size_t size, const void *caller);
void *mb_realloc(void *ptr, size_t size, const void *caller);
void mb_free(void *p, const void *caller);
void mb_log(const char *fmt, ...);

static const char *log_pathname = 0;
static int call_limit = 0;
static size_t mem_limit = 0;

static int str2int(const char *s);
static void init_env(void);
static FILE *open_stream(void);
static void close_stream(void);

static void *(*old_malloc_hook)(size_t size, const void *caller);
static void *(*old_realloc_hook)(void *ptr, size_t size, const void *caller);
static void (*old_free_hook)(void *ptr, const void *caller);

static void malloc_init(void);
static void print_trace(void);

void (*__malloc_initialize_hook)(void) = malloc_init;
static int debug_mode = 1;
static int backtrace_mode = 0;
static int abort_mode = 0;

static void
init_env(void)
{
  char *p;
  p = getenv("MB_MLIMIT");
  if (p)
    mem_limit = (size_t)str2int(p);

  p = getenv("MB_CLIMIT");
  if (p)
    call_limit = str2int(p);

  p = getenv("MB_LOGNAME");
  if (p)
    log_pathname = p;
  else
    log_pathname = "mb.out";

  p = getenv("MB_ABORT");
  if (p)
    abort_mode = str2int(p);

  p = getenv("MB_BACKTRACE");
  if (p) {
    backtrace_mode = str2int(p);
    if (backtrace_mode > MAX_BACKTRACE)
      backtrace_mode = MAX_BACKTRACE;
  }

  if (debug_mode) {
    fprintf(stderr, " mem_limit: %zu\n", mem_limit);
    fprintf(stderr, "call_limit: %d\n", call_limit);
    fprintf(stderr, "  log path: %s\n", log_pathname);
  }
}


void *
mb_malloc(size_t size, const void *caller)
{
  void *p;
  struct mb_head *h;

  LOCK();
  RESTORE_HOOK();

  assert(size > 0);

  if (mem_limit && mb_allocated + size > mem_limit)
    p = 0;
  else if (call_limit && malloc_called + realloc_called > call_limit)
    p = 0;
  else
    p = malloc(size + HEADER_SIZE);

  malloc_called++;
  if (p) {
    mb_allocated += size;
    SET_PEAK();

    h = (struct mb_head *)p;
    h->size = size;
    h->created = 0;
    h->modified = 0;
  }

  print_trace();

  mb_log("%10zd: malloc(%zu) from %p => %p",
         (ssize_t)size, size, caller, p ? p + HEADER_SIZE : 0);

  SAVE_HOOK();
  UNLOCK();

  return p ? p + HEADER_SIZE : 0;
}


void *
mb_realloc(void *ptr, size_t size, const void *caller)
{
  void *p;
  struct mb_head *h = HEADER(ptr);
  size_t oldsize;

  LOCK();
  RESTORE_HOOK();

  realloc_called++;

  if (!ptr) {
    assert(size > 0);

    if ((mem_limit && mb_allocated + size > mem_limit) ||
        (call_limit && malloc_called + realloc_called > call_limit))
      p = 0;
    else
      p = malloc(size + HEADER_SIZE);

    if (p) {
      mb_allocated += size;
      SET_PEAK();
    }
    mb_log("%10zd: realloc(%p, %zu) from %p => %p", (ssize_t)size,
           ptr, size, caller, p ? p + HEADER_SIZE : 0);

    SET_HEAD(p, size);
    SAVE_HOOK();
    UNLOCK();
    return p ? (unsigned char *)p + HEADER_SIZE : 0;
  }

  if (h->size == size) {
    mb_log("%10zd: realloc(%p, %zu) from %p => %p", (ssize_t)0,
           ptr, size, caller, p + HEADER_SIZE);

    if (call_limit && malloc_called + realloc_called > call_limit)
      ptr = 0;

    SAVE_HOOK();
    UNLOCK();
    return ptr;
  }

  oldsize = h->size;

  if ((mem_limit && mb_allocated + size - oldsize > mem_limit) ||
      (call_limit && malloc_called + realloc_called > call_limit))
    p = 0;
  else
    p = realloc((unsigned char *)ptr - HEADER_SIZE, size + HEADER_SIZE);

  if (p) {
    mb_allocated += size - oldsize;
    SET_PEAK();
  }

  mb_log("%10zd: realloc(%p, %zu) from %p => %p",
         (int)(size - oldsize), ptr, size, caller, p ? p + HEADER_SIZE : 0);
  SET_HEAD(p, size - oldsize);

  SAVE_HOOK();
  UNLOCK();

  return p ? (unsigned char *)p + HEADER_SIZE : 0;
}


void
mb_free(void *p, const void *caller)
{
  struct mb_head *h = HEADER(p);
  size_t oldsize;

  LOCK();
  RESTORE_HOOK();

  free_called++;
  if (p) {
    mb_allocated -= h->size;
    oldsize = h->size;
    free(h);
    mb_log("%10zd: free(%p) from %p", -oldsize, p, caller);
  }
  else
    mb_log("%10zd: free(%p) from %p", (ssize_t)0, p, caller);
  SAVE_HOOK();
  UNLOCK();
}


#ifdef _GNU_SOURCE

char *mb_strndup(const char *s, size_t n);
char *mb_strdupa(const char *s);
char *mb_strndupa(const char *s, size_t n);

#endif  /* _GNU_SOURCE */


static void
close_stream(void)
{
  RESTORE_HOOK();

  fprintf(mb_stream, "#\n");
  fprintf(mb_stream, "# Summary -------------------\n");
  fprintf(mb_stream, "#\n");
  fprintf(mb_stream, "# peak: %zu byte(s)\n", mb_alloc_peak);
  fprintf(mb_stream, "# malloc called %d times\n", malloc_called);
  fprintf(mb_stream, "# realloc called %d times\n", realloc_called);
  fprintf(mb_stream, "# free called %d times\n", free_called);
  fprintf(mb_stream, "#\n");

  if (mb_stream && mb_stream != stderr)
    fclose(mb_stream);
}


static FILE *
open_stream(void)
{
  FILE *fp;
  time_t tm;
  struct tm *tmptr;

  char buf[32];

  remove(log_pathname);

  time(&tm);
  tmptr = localtime(&tm);
  strftime(buf, 32, "%c", tmptr);

  fp = fopen(log_pathname, "a");
  if (!fp)
    return stderr;

  setbuf(fp, NULL);
  atexit(close_stream);

  fprintf(fp, "#\n");
  fprintf(fp, "# Automatically generated by mbuddy (%s)\n", buf);
  fprintf(fp, "#\n");
  fprintf(fp, "# Syntax: # comments\n");
  fprintf(fp,
          "#         \"TIME(msec): TOTAL-SIZE: "
          "ALLOC-INCREASE/DECREASE: MESSAGE\"\n");;
  fprintf(fp, "#\n");

  return fp;
}


void
mb_log(const char *fmt, ...)
{
  va_list ap;
  struct timeval tv;

  if (!mb_stream) {
    init_env();
    mb_stream = open_stream();
  }

  va_start(ap, fmt);

  if (gettimeofday(&tv, NULL) < 0)
    fprintf(stderr, "gettimeofday(2) failed: %s", strerror(errno));
  else {
    fprintf(mb_stream,
#if 0
            "%10lu:%06u.%06u:%10zu:",
            ++seq,
#else
            "%06u.%06u:%10zu:",
#endif  /* 0 */
            (unsigned)tv.tv_sec, (unsigned)tv.tv_usec, mb_allocated);
  }

  vfprintf(mb_stream, fmt, ap);
  fputc('\n', mb_stream);
  fflush(mb_stream);
  va_end(ap);
}


static int
str2int(const char *s)
{
  const char *p;
  char *q;
  int val;

  p = s + strspn(s, " \v\t\n\r");

  val = strtol(p, &q, 0);
  if (q) {
    switch (*q) {
    case 'm':
    case 'M':
      val *= 1024 * 1024;
      break;
    case 't':
    case 'T':
      val *= 1024 * 1024 * 1024;
      break;
    case 'k':
    case 'K':
      val *= 1024;
      break;
    default:
      break;
    }
  }

  return val;
}


static void
malloc_init(void)
{
  old_malloc_hook = __malloc_hook;
  old_realloc_hook = __realloc_hook;
  old_free_hook = __free_hook;

  __malloc_hook = mb_malloc;
  __realloc_hook = mb_realloc;
  __free_hook = mb_free;
}


static void
print_trace(void)
{
  void *array[MAX_BACKTRACE];
  size_t size;
  char **strings;
  size_t i;

  if (backtrace_mode == 0)
    return;

  if (!mb_stream) {
    init_env();
    mb_stream = open_stream();
  }

  size = backtrace(array, backtrace_mode);
  strings = backtrace_symbols(array, size);

  fprintf(mb_stream, "# Obtained %zd stack frames.\n", size);

  for (i = 0; i < size; i++)
    fprintf(mb_stream, "# %d: %s\n", i, strings[i]);

  free (strings);
}


#ifdef TEST_MBUDDY

void car()
{
  print_trace();
}

void bar()
{
  car();
}

void
foo()
{
  bar();
}

int
main(int argc, char *argv[])
{
  void *p;

  if (argc >= 2) {
    printf("int: %d\n", str2int(argv[1]));
  }

  //mb_stream = stderr;

  //foo();
  p = malloc(10);

  p = realloc(p, 20);
  p = realloc(p, 10);

  free(p);

  return 0;
}
#endif  /* TEST_MBUDDY */

