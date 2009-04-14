/* $Id$ */
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
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <errno.h>

#define DLOG_BUILD
#include "dlog.h"

#ifndef NO_THREAD
# include <pthread.h>
#else
# define flockfile(fp)          ((void)0)
# define funlockfile(fp)        ((void)0)
#endif  /* NO_THREAD */

#define DLOG_THREAD_NUM 128

static FILE *dlog_fp;
static unsigned dlog_mask;
static const char *dlog_prefix = NULL;


static int dlog_thread = 0;

#ifndef NO_THREAD
static int dlog_thcounter;
static pthread_once_t thinfo_once = PTHREAD_ONCE_INIT;
static pthread_key_t key_thname;
static pthread_mutex_t mutex_thname = PTHREAD_MUTEX_INITIALIZER;

static void free_thname(void *value);
static void dlog_thread_init_once(void);
static char *dlog_create_name(void);
#endif  /* NO_THREAD */


static const char *dlog_get_thread_name(void);

const char *program_name __attribute__((weak)) = NULL;
//const char *program_name = NULL;


FILE *
dlog_set_stream(FILE *fp)
{
  FILE *old = dlog_fp;

  dlog_fp = fp;

  return old;
}


static void
dlog_close_stream(void)
{
  if (dlog_fp)
    fclose(dlog_fp);
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


void
derror(int ecode, int status, unsigned category, const char *format, ...)
{
  va_list ap;
  int fd;
  int ret;
  struct flock lock;

  if (dlog_fp) {
    if (dlog_mask & category)
      return;

    fflush(dlog_fp);
#ifdef FLOCK
    fd = fileno(dlog_fp);

    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    do {
      ret = fcntl(fd, F_SETLK, &lock);
      if (ret == 0)
        break;
      if (errno != EAGAIN && errno != EAGAIN) {
        fprintf(stderr, "dlog: fcntl() failed on set: %s\n", strerror(errno));
        abort();
      }
      else {
        //fprintf(stderr, "dlog: locked. try again.\n");
      }
    } while (1);
#endif  /* FLOCK */

    //flockfile(stdout);
    fflush(stdout);
    flockfile(dlog_fp);

    if (dlog_prefix)
      fprintf(dlog_fp, "%s: ", dlog_prefix);

    if (dlog_thread)
      fprintf(dlog_fp, "<%d-%s>: ", (unsigned int)getpid(),
              dlog_get_thread_name());

    if (status)
      fprintf(dlog_fp, "%s: ", strerror(status));

    va_start(ap, format);
    vfprintf(dlog_fp, format, ap);
    va_end(ap);
    fputc('\n', dlog_fp);

    fflush(dlog_fp);
    funlockfile(dlog_fp);
    //funlockfile(stdout);

#ifdef FLOCK
    lock.l_type = F_UNLCK;
    ret = fcntl(fd, F_SETLK, &lock);
    if (ret != 0) {
      fprintf(stderr, "dlog: fcntl() failed on release: %s\n", strerror(errno));
      abort();
    }
#endif  /* FLOCK */

    fflush(dlog_fp);
  }

  if (ecode)
    exit(ecode);
}


#if 0
void
derror(int ecode, int status, const char *format, ...)
{
  va_list ap;

  if (!dlog_fp)
    dlog_set_stream(0);

  flockfile(stdout);
  flockfile(dlog_fp);
  fflush(stdout);
  fprintf(dlog_fp, "%s: ", dlog_prefix);

  if (status)
    fprintf(dlog_fp, "%s: ", strerror(status));

  va_start(ap, format);
  vfprintf(dlog_fp, format, ap);
  va_end(ap);
  fputc('\n', dlog_fp);

  fflush(stderr);
  funlockfile(dlog_fp);
  funlockfile(stdout);

  if (ecode)
    exit(ecode);
}
#endif  /* 0 */


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
  dlog_thcounter = 0;
  if (pthread_key_create(&key_thname, free_thname) < 0)
    fprintf(stderr, "dlog: cannot create a thread key: %s\n",
            strerror(errno));
}


static char *
dlog_create_name(void)
{
  char *name = NULL;
#ifndef NO_THREAD
  int id;

  pthread_mutex_lock(&mutex_thname);
  id = ++dlog_thcounter;
  pthread_mutex_unlock(&mutex_thname);

  if (asprintf(&name, "thread#%u", id) < 0)
    return NULL;
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
  FILE *fp;

  dlog_set_stream(stderr);
  p = getenv("DLOG_FILE");
  if (p) {
    if (p[0] == '\0')
      dlog_set_stream(0);
    else
      fp = fopen(p, "w");

    if (!fp)
      fprintf(stderr, "warning: cannot open file in DLOG_FILE.\n");
    else
      dlog_set_stream(fp);
  }
  atexit(dlog_close_stream);

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
  if (pthread_once(&thinfo_once, dlog_thread_init_once) != 0)
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

  p = pthread_getspecific(key_thname);
  if (p)
    free(p);

  if (pthread_setspecific(key_thname, name) != 0)
    return -1;
#endif  /* NO_THREAD */

  return 0;
}


static const char *
dlog_get_thread_name(void)
{
  char *p = NULL;

#ifndef NO_THREAD
  p = pthread_getspecific(key_thname);
  if (!p) {
    p = dlog_create_name();
    dlog_set_thread_name(p);
    free(p);
  }
  p = pthread_getspecific(key_thname);
#endif  /* NO_THREAD */

  return p;
}


#ifdef TEST_DLOG
int
main(void)
{
  dlog_init();
  dlog_thread_init();
  dlog_thread_init();

  dlog(0, 0, 0x1, "hello %s.", "world");
  dlog(0, 0, 0x2, "hi %s.", "world");

  return 0;
}
#endif  /* TEST_DLOG */
