
/* mbuddy: malloc debugging buddy
 * Copyright (C) 2006  Seong-Kook Shin <cinsky@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <sys/time.h>
#include <pthread.h>

#include <malloc.h>
#include <execinfo.h>

#include <mbuddy.h>

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
                          __malloc_hook = mb_malloc_; \
                          __realloc_hook = mb_realloc_; \
                          __free_hook = mb_free_; \
                        } while (0)

#define SET_PEAK()      do { \
                          if (mb_allocated > mb_alloc_peak) \
                            mb_alloc_peak = mb_allocated; \
                        } while (0)

static void *mb_malloc_(size_t size, const void *caller);
static void *mb_realloc_(void *ptr, size_t size, const void *caller);
static void mb_free_(void *p, const void *caller);
static void mb_log(const char *fmt, ...);

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

#define STATS_MAX       17

int stats[STATS_MAX];


void
add_stats(size_t size)
{
  int i;
  for (i = STATS_MAX - 1; i >= 0; i--)
    if (size >= (1 << i)) {
      stats[i]++;
      break;
    }
}


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
    log_pathname = MB_LOGNAME;

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
mb_malloc(size_t size)
{
  return mb_malloc_(size, 0);
}

void *
mb_realloc(void *ptr, size_t size)
{
  return mb_realloc_(ptr, size, 0);
}


void
mb_free(void *p)
{
  return mb_free_(p, 0);
}


static void *
mb_malloc_(size_t size, const void *caller)
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
    add_stats(size);
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


static void *
mb_realloc_(void *ptr, size_t size, const void *caller)
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
      add_stats(size);
      SET_PEAK();
    }
    print_trace();
    mb_log("%10zd: realloc(%p, %zu) from %p => %p", (ssize_t)size,
           ptr, size, caller, p ? p + HEADER_SIZE : 0);

    SET_HEAD(p, size);
    SAVE_HOOK();
    UNLOCK();
    return p ? (unsigned char *)p + HEADER_SIZE : 0;
  }

  if (h->size == size) {
    print_trace();
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
    add_stats(size);
    SET_PEAK();
  }

  print_trace();
  mb_log("%10zd: realloc(%p, %zu) from %p => %p",
         (int)(size - oldsize), ptr, size, caller, p ? p + HEADER_SIZE : 0);
  SET_HEAD(p, size - oldsize);

  SAVE_HOOK();
  UNLOCK();

  return p ? (unsigned char *)p + HEADER_SIZE : 0;
}


static void
mb_free_(void *p, const void *caller)
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
    print_trace();
    mb_log("%10zd: free(%p) from %p", -oldsize, p, caller);
  }
  else {
    print_trace();
    mb_log("%10zd: free(%p) from %p", (ssize_t)0, p, caller);
  }
  SAVE_HOOK();
  UNLOCK();
}


#ifdef _GNU_SOURCE

char *mb_strndup(const char *s, size_t n);
char *mb_strdupa(const char *s);
char *mb_strndupa(const char *s, size_t n);

#endif  /* _GNU_SOURCE */


int
ndigit(size_t size)
{
  int n = 0;

  if (size == 0)
    return 1;

  while (size > 0) {
    size /= 10;
    n++;
  }

  return n;
}


void
print_stats(void)
{
  int i, j, sum = 0;
  double percent;

  fprintf(mb_stream, "# Statistics ----------------\n#\n");

  for (i = 0; i < STATS_MAX; i++)
    sum += stats[i];

  fprintf(mb_stream, "#  Total %d block(s) are allocated.\n#\n", sum);

  for (i = 0; i < STATS_MAX; i++) {
    fprintf(mb_stream, "#  >= %5u byte(s) block: ", 1 << i);

    percent = (double)stats[i] / sum * 100.0;
    fprintf(mb_stream, "%*d (%2d%%): ",
            ndigit(sum), stats[i], (int)ceil(percent));

    j = (int)ceil(percent * 0.4);
    while (j-- > 0)
      fputc('*', mb_stream);
    fputc('\n', mb_stream);
  }
  fputc('\n', mb_stream);
  fflush(mb_stream);
}

static void
close_stream(void)
{
  RESTORE_HOOK();

  fprintf(mb_stream, "#\n");
  fprintf(mb_stream, "# Summary -------------------\n#\n");
  fprintf(mb_stream, "#  peak: %zu byte(s)\n", mb_alloc_peak);
  fprintf(mb_stream, "#  malloc called %d times\n", malloc_called);
  fprintf(mb_stream, "#  realloc called %d times\n", realloc_called);
  fprintf(mb_stream, "#  free called %d times\n", free_called);
  fprintf(mb_stream, "#\n");

  print_stats();

  if (mb_stream && mb_stream != stderr)
    fclose(mb_stream);
}


static const char *help_msgs[] = {
  "",
  " A line starting with '#' character is a comment.",
  " The syntax of each record is",
  "",
  "   TIME(msec): TOTAL-SIZE: INC/DEC: MESSAGE",
  "",
  " 1) TIME is the time in microseconds when the MESSAGE is about to happen.",
  " 2) TOTAL-SIZE is the accumulated size of dynamically allocated memory ",
  "    in bytes.",
  " 3) INC/DEC is the amount of increasement/decreasement of dynamically",
  "    allocated memory in this moments.  The positive value means that some",
  "    memory is allocated, and the negative value means that some memory is",
  "    deallocated.",
  " 4) MESSAGE tells that the description of each memory related operation.",
  "    For example, if the program calls malloc(3), the message looks like:",
  "",
  "      malloc(676) from 0xb7efcdce => 0x805f018",
  "",
  "    This means that malloc(3) tries to allocate 676 bytes from the caller",
  "    0xb7efcdce, and malloc(3) returns the pointer 0x805f018.",
  "",
  "",
  " Users may set some of the environment varibles to control mbuddy output.",
  "",
  " MB_MLIMIT -- If set to nonzero integer value, allocators will be",
  "              failed when the total amount of allocated memory is",
  "              greater than this value.  You may use 'K', 'M', 'T'",
  "              postfix to set the value.  (e.g. \"1024K\" means 1024",
  "              kilobytes, \"3M\" means 3 megabytes, \"1T\" means 1",
  "              terabytes.)",
  "              If set to zero, memory limit is turned off.",
  "             ",
  " MB_CLIMIT -- If set to nonzero integer value, calling allocators",
  "              more than MB_CLIMIT times will be failed.",
  "              Other conditions of MB_MLIMIT are preserved.",
  "",
  " MB_LOGNAME - The output filename. If not set, \"mb.out\" will be ",
  "              used by default.",
  "",
  " MB_ABORT --  If set to nonzero value, and if the allocators failed,",
  "              the allocators automatically call abort(3).  Might be",
  "              useful in gdb session.",
  "",
  " MB_BACKTRACE If set to nonzero value, the backtrace information preceeds",
  "              each record.  For example, if MB_BACKTRACE is set to 10, ",
  "              mbuddy will show 10 caller information in a comment.",
  "              The recommended value is 10.",
  "",
  NULL,
};


void
print_header(FILE *fp)
{
  time_t tm;
  struct tm *tmptr;
  char buf[32];
  int i;

  time(&tm);
  tmptr = localtime(&tm);
  strftime(buf, 32, "%c", tmptr);

  fprintf(fp, "# -*-conf-*-\n");
  fprintf(fp, "#\n");
  fprintf(fp, "# Automatically generated by mbuddy (%s)\n", buf);
  fprintf(fp, "#\n");

  for (i = 0; help_msgs[i]; i++) {
    fprintf(fp, "# %s\n", help_msgs[i]);
  }

  fprintf(fp, "#\n");
  fflush(fp);
}


static FILE *
open_stream(void)
{
  FILE *fp;

  remove(log_pathname);

  fp = fopen(log_pathname, "a");
  if (!fp)
    return stderr;

  setbuf(fp, NULL);
  atexit(close_stream);

  print_header(fp);

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

  __malloc_hook = mb_malloc_;
  __realloc_hook = mb_realloc_;
  __free_hook = mb_free_;
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
    fprintf(mb_stream, "# [BT] %d: %s\n", i, strings[i]);

  free (strings);
}


#ifdef TEST_MBUDDY

typedef void (*func_t)(int);


#define MAX_FUNC        4
#define MAX_DEPTH       4
#define MAX_SLOT        256

#define ALLOC_MAX       (65 * 1024)

void foo(int depth);
void bar(int depth);
void car(int depth);
void cdr(int depth);

func_t functions[MAX_FUNC] = { foo, bar, car, cdr, };
int dir = 1;

void *ptrs[MAX_SLOT] = { 0, };


size_t
alloc_size()
{
  int a[8] = {
    rand() % (ALLOC_MAX / 1000),
    rand() % (ALLOC_MAX / 800),
    rand() % (ALLOC_MAX / 500),
    rand() % (ALLOC_MAX / 300),
    rand() % (ALLOC_MAX / 150),
    rand() % (ALLOC_MAX / 100),
    rand() % (ALLOC_MAX / 10),
    rand() % (ALLOC_MAX),
  };
  int i = rand() % 8;

  return a[i] > 0 ? a[i] : 1;
}


void
foo(int depth)
{
  int index = rand() % MAX_SLOT;

  printf("foo(%d)\n", depth);

  if (ptrs[index]) {
    free(ptrs[index]);
    ptrs[index] = 0;
  }

  ptrs[index] = malloc(alloc_size());

  if (depth < MAX_DEPTH && rand() % 2) {
    (*functions[rand() % MAX_FUNC])(depth + 1);
  }

  if (rand() % 4 == 0) { /* 3/4 ratio, free the resource */
    free(ptrs[index]);
    ptrs[index] = 0;
  }
}


void
bar(int depth)
{
  int index = rand() % MAX_SLOT;

  printf("bar(%d)\n", depth);

  ptrs[index] = realloc(ptrs[index], alloc_size());

  if (depth < MAX_DEPTH && rand() % 2) {
    (*functions[rand() % MAX_FUNC])(depth + 1);
  }

  if (rand() % 2) { /* 3/4 ratio, free the resource */
    free(ptrs[index]);
    ptrs[index] = 0;
  }
}


void
car(int depth)
{
  int index = rand() % MAX_SLOT;

  printf("car(%d)\n", depth);

  if (rand() % 5 == 0)
    dir = !dir;

  if (dir) {
    int amount = alloc_size();
    printf("car(%d) -- resizing ptrs[%d] to %d\n", depth, index, amount);
    ptrs[index] = realloc(ptrs[index], amount);
  }
  else {
    printf("car(%d) -- freeing ptrs[%d]\n", depth, index);
    free(ptrs[index]);
    ptrs[index] = 0;
  }
}


void
cdr(int depth)
{
  int i = rand() % MAX_SLOT;
  int count = 0;
  int index;

  while (i-- > 0) {
    index = rand() % MAX_SLOT;
    if (ptrs[index]) {
      count++;
      free(ptrs[index]);
      ptrs[index] = 0;
    }
  }
  printf("cdr(%d) -- releasing maximum %d slot(s)\n", depth, i);
}


int
main(int argc, char *argv[])
{
  int i;

  srand(time(0));

  if (argc != 2) {
    fprintf(stderr, "usage: %s [CALLS]\n", argv[0]);
    return 1;
  }
  i = atoi(argv[1]);

  while (i-- > 0)
    foo(0);

  malloc(1);
  malloc(69999);
  return 0;
}
#endif  /* TEST_MBUDDY */


