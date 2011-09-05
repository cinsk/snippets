/* dlog: message logger for debugging
 * Copyright (C) 2009  Seong-Kook Shin <cinsky@gmail.com>
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
#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>

#include <snprintx.h>


#define DLOG_BUILD
#include "dlog.h"

#ifndef NO_THREAD
# include <pthread.h>
#else
# define flockfile(fp)          ((void)0)
# define funlockfile(fp)        ((void)0)
#endif  /* NO_THREAD */

#define DLOG_THREAD_NUM 128

#define HAVE_GETTID
#define USE_GETTID     1

#if defined(HAVE_GETTID) && defined(USE_GETTID) && (USE_GETTID > 0)
#include <sys/syscall.h>
#endif

#define FILE_MODE       (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

static int dlog_fd = -1;
static unsigned dlog_mask;
static const char *dlog_prefix = NULL;


static int dlog_thread = 0;     /* If zero, disable thread support */
static int dlog_trace_child = 0; /* If nonzero, print PID of each process */

#ifndef NO_THREAD
struct thinfo_ {
  int counter;                  /* for naming each thread */
  pthread_once_t once;          /* once control for the initializer */
  pthread_mutex_t mutex;        /* mutex for `counter' member */
  pthread_key_t key;            /* thread-specific data for thread names */
};

static struct thinfo_ thinfo = {
  0,
  PTHREAD_ONCE_INIT,
  PTHREAD_MUTEX_INITIALIZER,
};

static void free_thname(void *value);
static void dlog_thread_init_once(void);
static char *dlog_create_name(void);
#endif  /* NO_THREAD */


static const char *dlog_get_thread_name(void);

const char *program_name __attribute__((weak)) = NULL ;


int
dlog_set_file(int fd)
{
  int old = dlog_fd;

  if (dlog_fd != fd) {
    if (dlog_fd != -1)
      close(dlog_fd);

    if (fd != -1) {
      fsync(fd);
      if (fcntl(fd, F_SETFL, O_APPEND) == -1)
        return -1;

      dlog_fd = fd;
    }
  }
  return old;
}


static void
dlog_close_file(void)
{
  if (dlog_fd != -1)
    close(dlog_fd);
}


unsigned
dlog_set_filter(unsigned mask)
{
  unsigned old;

  old = dlog_mask;
  dlog_mask = mask;

  return old;
}


const char *
dlog_set_prefix(const char *prefix)
{
  const char *old = dlog_prefix;
  dlog_prefix = prefix;
  return old;
}


#if !defined(NO_THREAD) && defined(USE_GETTID) && (USE_GETTID > 0)
static pid_t
gettid(void)
{
  return syscall(SYS_gettid);
}
#endif  /* USE_GETTID */

void
derror(int status, int errnum, unsigned category, const char *format, ...)
{
  va_list ap;
  char buf[PIPE_BUF];
  char *ptr = buf;
  ssize_t bufsize = sizeof(buf);

  if (dlog_fd != -1) {
    if (dlog_mask & category)
      return;

    fflush(stdout);
    if (dlog_fd == STDERR_FILENO)
      fflush(stderr);

    if (dlog_prefix)
      snprintx(&ptr, &bufsize, "%s: ", dlog_prefix);

    if (dlog_thread && dlog_trace_child)
      snprintx(&ptr, &bufsize, "<%d:%s> ", (unsigned int)getpid(),
              dlog_get_thread_name());
    else if (dlog_thread)
      snprintx(&ptr, &bufsize, "<:%s> ", dlog_get_thread_name());
    else if (dlog_trace_child)
      snprintx(&ptr, &bufsize, "<%d:> ", (unsigned int)getpid());

    va_start(ap, format);
    vsnprintx(&ptr, &bufsize, format, ap);
    va_end(ap);

    if (errnum)
      vsnprintx(&ptr, &bufsize, ": %s", strerror(errnum));

    snprintx(&ptr, &bufsize, "\n");

    if (bufsize > 0)
      write(dlog_fd, buf, sizeof(buf) - bufsize);

    fsync(dlog_fd);
  }

  if (status)
    exit(status);
}


#ifndef NO_THREAD
static void
free_thname(void *value)
{
  if (value)
    free(value);
}


static void
dlog_thread_init_once(void)
{
  thinfo.counter = 0;
  if (pthread_key_create(&thinfo.key, free_thname) < 0)
    fprintf(stderr, "dlog: cannot create a thread key: %s\n",
            strerror(errno));
}


static char *
dlog_create_name(void)
{
  char *name = NULL;
#ifndef NO_THREAD

#if defined(USE_GETTID) && (USE_GETTID > 0)
  if (asprintf(&name, "%u", (unsigned)gettid()) < 0)
    return NULL;
#else
  int id;

  pthread_mutex_lock(&thinfo.mutex);
  id = ++thinfo.counter;
  pthread_mutex_unlock(&thinfo.mutex);

  if (asprintf(&name, "thread#%u", id) < 0)
    return NULL;
#endif  /* USE_GETTID */

#endif  /* NO_THREAD */

  return name;
}
#endif  /* NO_THREAD */


int
dlog_init(void)
{
  const char *p;
  char *endptr = NULL;
  unsigned int mask;
  int fd;
  int tracep;

  dlog_set_file(STDERR_FILENO);
  p = getenv("DLOG_FILE");
  if (p) {
    if (p[0] == '\0')
      dlog_set_file(-1);
    else if (strcmp(p, "-") == 0)
      dlog_set_file(STDOUT_FILENO);
    else {
      fd = open(p, O_WRONLY | O_CREAT | O_APPEND | O_TRUNC, FILE_MODE);

      if (fd == -1)
        fprintf(stderr, "warning: cannot open file in DLOG_FILE.\n");
      else
        dlog_set_file(fd);
    }
  }
  atexit(dlog_close_file);

  p = getenv("DLOG_MASK");
  if (p && p[0] != '\0') {
    mask = strtoul(p, &endptr, 0);
    if (*endptr == '\0') {
      dlog_mask = mask;
    }
    else {
      if (program_name)
        fprintf(stderr, "%s: ", program_name);
      fprintf(stderr, "warning: unrecognized format in DLOG_MASK.\n");
    }
  }

  p = getenv("DLOG_TRACE_CHILD");
  if (p) {
    if (p[0] != '\0') {
      tracep = strtoul(p, &endptr, 0);
      if (*endptr == '\0')
        dlog_trace_child = (tracep) ? 1 : 0;
      else {
        if (program_name)
          fprintf(stderr, "%s: ", program_name);
        fprintf(stderr, "warning: unrecognized format in DLOG_TRACE_CHILD.\n");
      }
    }
    else
      dlog_trace_child = 0;
  }

  if (program_name)
    dlog_set_prefix(program_name);

  return 0;
}


int
dlog_thread_init(void)
{
  int ret = -1;

#ifndef NO_THREAD
  ret = 0;
  dlog_thread = 1;
  if (pthread_once(&thinfo.once, dlog_thread_init_once) != 0)
    return -1;
#endif  /* NO_THREAD */

  return ret;
}




int
dlog_set_thread_name(const char *name)
{
#ifndef NO_THREAD
  char *p;

  name = strdup(name);
  if (!name)
    return -1;

  p = pthread_getspecific(thinfo.key);
  free_thname(p);

  if (pthread_setspecific(thinfo.key, name) != 0)
    return -1;
#endif  /* NO_THREAD */

  return 0;
}


static const char *
dlog_get_thread_name(void)
{
  char *p = NULL;

#ifndef NO_THREAD
  p = pthread_getspecific(thinfo.key);
  if (!p) {
    p = dlog_create_name();
    dlog_set_thread_name(p);
    free(p);
  }
  p = pthread_getspecific(thinfo.key);
#endif  /* NO_THREAD */

  return p;
}


#ifdef TEST_DLOG
int
main(void)
{
  dlog_init();
  dlog_thread_init();
  //dlog_thread_init();

  dlog(0, 0x1, "hello %s.", "world");
  dlog(0, 0x2, "hi %s.", "world");

  return 0;
}
#endif  /* TEST_DLOG */
