/* writepid: easy function to create .pid file.
 * Copyright (C) 2010  Seong-Kook Shin <cinsky@gmail.com>
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
#define _GNU_SOURCE     1

#if 0
//#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "fileutil.h"
#include "writepid.h"

/* To use O_DIRECT, I need to handle EINVAL, etc.  */
#define OPENFLAGS      (O_WRONLY | O_CREAT | O_SYNC | O_TRUNC)

#define OPENPERMS       (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

/* PIDBUF_MAX defines the buffer size that stores the stringed PID number. */
#define PIDBUF_MAX      32


int
writepid(const char *pathname, pid_t pid)
{
  int fd, ret, tmp;
  char buf[PIDBUF_MAX] = { 0, };
  int saved_errno = errno;

  if (make_directory(pathname) != 0)
    return -1;

  if (pid == (pid_t)-1)
    pid = getpid();

  fd = open(pathname, OPENFLAGS, OPENPERMS);
  if (fd == -1)
    return -1;

  ret = snprintf(buf, PIDBUF_MAX - 1, "%lu\n", (unsigned long)pid);
  if (ret >= PIDBUF_MAX - 1) {
    /* Fatal error: output is truncated.
     * If the control flows here, you need to enlarge PIDBUF_MAX.
     */
    assert(0);
  }

 again:
  if ((tmp = write(fd, buf, ret)) < 0) {
    if (errno == EINTR) {
      /* I'm not sure write(2) again is a good idea, though... */
      goto again;
    }
    else {
      saved_errno = errno;
      close(fd);
      errno = saved_errno;
      return -1;
    }
  }

  fsync(fd);                    /* unnecessary, perhaps?  */

  if (close(fd) != 0)
    return -1;

  return 0;
}
#endif  /* 0 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>

#ifdef XERROR
#include "xerror.h"
#endif

#include "writepid.h"

static const char *writepid_pathname = 0;
static struct stat writepid_stat;

char *writepid_mkdir_template __attribute__((weak)) = WRITEPID_MKDIR_TEMPLATE;

static void
remove_pidfile(void)
{
  struct stat sbuf;

  if (!writepid_pathname)
    return;

  if (stat(writepid_pathname, &sbuf) == -1)
    goto err;

  if (sbuf.st_ino == writepid_stat.st_ino &&
      sbuf.st_size == writepid_stat.st_size &&
      sbuf.st_mtime == sbuf.st_mtime) {
    /* Assuming that WRITEPID_PATHNAME is the same as before */
    /* Note that there is no way to guarantee that WRITEPID_PATHNAME
     * is *the file* that this process had written. */
    unlink(writepid_pathname);
  }
  else {
#ifdef XERROR
    xdebug(0, "pidfile(%s) seems not the same as it used to be, leaving it",
           writepid_pathname);
#endif
  }

 err:
  free((void *)writepid_pathname);
}


int
writepid(const char *pidfile, pid_t pid, int no_atexit)
{
  // Warning: PIDFILE must be canonicalized pathname.
  int ret = -1;
  int saved_errno;
  char *cmdline = 0;
  char *dname, *dname_ = strdup(pidfile);
  FILE *fp;

  if (!dname_)
    return -1;

  dname = dirname(dname_);
  if (!dname) {
    saved_errno = errno;
    goto err;
  }

  if (asprintf(&cmdline, writepid_mkdir_template, dname) == -1) {
    saved_errno = errno;
    goto err;
  }

  ret = system(cmdline);
  if (ret == -1 || ret == 127) {
    saved_errno = errno;
    free(cmdline);
    goto err;
  }
  else if (WIFEXITED(ret) && WEXITSTATUS(ret) == 0)
    ;                           /* pass */
  else {
#ifdef XERROR
    if (WIFSIGNALED(ret))
      xdebug(errno, "system(3): signal %d raised", WTERMSIG(ret));
    else if (WIFEXITED(ret))
      xdebug(errno, "system(3): nonzero exit status %d", WEXITSTATUS(ret));
#endif  /* XERROR */

    saved_errno = errno;
    free(cmdline);
    goto err;
  }


  free(cmdline);

  fp = fopen(pidfile, "w");
  if (!fp) {
    saved_errno = errno;
    goto err;
  }
  if (fprintf(fp, "%d\n", (pid > 0) ? (int)pid : (int)getpid()) < 0) {
    saved_errno = errno;
    fclose(fp);
    goto err;
  }
  fclose(fp);

  if (!no_atexit) {
    writepid_pathname = strdup(pidfile);
    if (!writepid_pathname) {
      saved_errno = errno;
      goto err;
    }
    if (stat(writepid_pathname, &writepid_stat) == -1) {
      saved_errno = errno;
      free((void *)writepid_pathname);
      writepid_pathname = 0;
      goto err;
    }

    if (atexit(remove_pidfile) == -1) {
      saved_errno = errno;
      free((void *)writepid_pathname);
      writepid_pathname = 0;
      goto err;
    }
  }

  return 0;

 err:
  free(dname_);
  errno = saved_errno;
  return -1;
}


#ifdef TEST_WRITEPID
#define SLEEP_INTERVAL  5

char *writepid_mkdir_template = "echo Ahhhhhhhhh";

int
main(int argc, char *argv[])
{
  if (argc != 2) {
    fprintf(stderr, "usage: %s PIDFILE\n", argv[0]);
    return 1;
  }

  fprintf(stderr, "writepid returns %d\n", writepid(argv[1], -1, 0));
  fprintf(stderr, "sleeping %d second(s)...\n", SLEEP_INTERVAL);
  sleep(SLEEP_INTERVAL);
  return 0;
}

#endif  /* TEST_WRITEPID */
