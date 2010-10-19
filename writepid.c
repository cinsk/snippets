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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "writepid.h"

#define DIR_SEPARATOR   '/'

#ifdef O_DIRECT
# define OPENFLAGS      (O_WRONLY | O_CREAT | O_SYNC | O_TRUNC | O_DIRECT)
#else
# define OPENFLAGS      (O_WRONLY | O_CREAT | O_SYNC | O_TRUNC)
#endif

#define OPENPERMS       (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

/* PIDBUF_MAX defines the buffer size that stores the stringed PID number. */
#define PIDBUF_MAX      32


static char *
dirname(char *pathname)
{
  size_t len = strlen(pathname);
  char *p = pathname + len;

  while (p >= pathname) {
    if (*p == DIR_SEPARATOR) {
      *p = '\0';
      break;
    }
    p--;
  }

  if (p < pathname) {           /* e.g. pathname = "asdf" */
    strcpy(pathname, ".");
  }
  else if (p == pathname) {     /* e.g. pathname = "/asdf" */
    strcpy(pathname, "/");
  }
  return pathname;
}


static int
make_directory(const char *pathname)
{
  char *path;
  size_t len;
  char *p;
  mode_t mask;
  char saved_char;
  int ret = -1;
  int saved_errno = errno;

  if (!pathname)
    return -1;

  len = strlen(pathname);
  if (len < 1)
    len = 1;
  path = malloc(len + 1);
  if (!path)
    return -1;
  strcpy(path, pathname);

  dirname(path);

  if (strcmp(path, ".") == 0 || strcmp(path, "/") == 0)
    return 0;

  mask = umask(0);
  umask(mask & ~0300);          /* ensure intermediate dirs are wx */

  p = path;
  while (1) {
    /* This loop tokenizing the pathname by replacing the first
     * non-'/' character into '\0'.  The replaced character is saved
     * in 'saved_char'. */
    saved_char = '\0';
    while (*p) {
      if (*p == '/') {
        while (*++p == '/')
          ;
        saved_char = *p;
        *p = '\0';
        break;
      }
      ++p;
    }

    if (!saved_char)
      umask(mask);

    if (mkdir(path, 0777) < 0) {
      if (errno != EEXIST) {
#ifdef TEST_WRITEPID
        fprintf(stderr, "mkdir: |%s| failed, errno = %d\n", path, errno);
#endif
        saved_errno = errno;
        goto end;
      }
      else {
#ifdef TEST_WRITEPID
        fprintf(stderr, "mkdir: |%s| exist\n", path);
#endif
      }
    }
    else {
#ifdef TEST_WRITEPID
      fprintf(stderr, "mkdir: |%s| success\n", path);
#endif
    }

    if (!saved_char)
      break;
    *p = saved_char;
  }
  ret = 0;

 end:
  free(path);
  umask(mask);
  errno = saved_errno;
  return ret;
}


int
writepid(const char *pathname, pid_t pid)
{
  int fd, ret;
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
  if (write(fd, buf, ret) < 0) {
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


#ifdef TEST_WRITEPID
int
main(int argc, char *argv[])
{
  if (argc != 2) {
    fprintf(stderr, "usage: %s PIDFILE\n", argv[0]);
    return 1;
  }

  fprintf(stderr, "writepid returns %d\n", writepid(argv[1], -1));
  return 0;
}

#endif  /* TEST_WRITEPID */
