/* $Id$ */
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/un.h>


/*
 * eXtended open(2)
 *
 * If the PATHNAME does not exist, xopen() behaves exactly the same as
 * open().
 *
 * If PATHNAME exists however, one of following cases happen:
 * - If it refers to a regular file, it calls open(2).
 * - If it refers a FIFO, xopen() opens the FIFO.  Unnecessary flags
 *   such as O_CREAT, O_TRUNC, or O_EXCL are removed from FLAGS in
 *   this case.
 * - If it refers to a socket file, xopen() connect(2)s to it.  Since
 *   xopen() does not know whether the socket file is a datagram
 *   socket or a stream socket, it tries one after another.  FLAGS are
 *   ignored in this case.
 * - If PATHNAME refers to other type of special file, it just call
 *   open(2).
 *
 */
int
xopen(const char *pathname, int flags, ...)
{
  struct stat statbuf;
  int fd;
  mode_t mode = 0;
  va_list ap;
  int saved_errno;

  if (stat(pathname, &statbuf) == -1) {
    if (errno == ENOENT)
      goto do_create;
    return -1;
  }

  if (S_ISREG(statbuf.st_mode) ) {
  do_create:
    if (flags & O_CREAT) {
      va_start(ap, flags);
      mode = (mode_t)va_arg(ap, mode_t);
      va_end(ap);
      fd = open(pathname, flags, mode);
    }
    else
      fd = open(pathname, flags);
    return fd;
  }
  else if (S_ISFIFO(statbuf.st_mode)) {
    flags = flags & (~(O_CREAT | O_TRUNC | O_EXCL));
    fd = open(pathname, flags);
    return fd;
  }
  else if (S_ISSOCK(statbuf.st_mode)) {
    struct sockaddr_un addr;
    if (strlen(pathname) >= sizeof(addr.sun_path)) {
      errno = ENAMETOOLONG;
      return -1;
    }
    fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (fd == -1)
      return -1;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_LOCAL;
    strcpy(addr.sun_path, pathname);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
      close(fd);
      if (errno != EPROTOTYPE)
        return -1;

      fd = socket(AF_LOCAL, SOCK_DGRAM, 0);
      if (fd == -1)
        return -1;
      if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
      }
    }
    return fd;
  }
  else {
    if (flags & O_CREAT) {
      va_start(ap, flags);
      mode = (mode_t)va_arg(ap, mode_t);
      va_end(ap);

      fd = open(pathname, flags, mode);
    }
    else
      fd = open(pathname, flags);
    return fd;
  }
}


#ifndef TEST_XOPEN
#include <stdio.h>

#define FILE_MODE       (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

int
main(int argc, char *argv[])
{
  int fd;

  fd = xopen(argv[1], O_RDWR | O_CREAT | O_TRUNC, FILE_MODE);
  if (fd == -1) {
    printf("errno(%d:%s)\n", errno, strerror(errno));
    return 1;
  }

  return 0;
}
#endif  /* TEST_XOPEN */
