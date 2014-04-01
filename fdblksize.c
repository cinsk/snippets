#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>

#define RCVBUF_SIZE     -1
#define SNDBUF_SIZE     -1

static void
fd_blk_size(int fd, int *rdbuf_size, int *wrbuf_size)
{
  int ibsz, obsz, notsock = 0, ret = 0;
  socklen_t bsz_len;
  struct stat sbuf;

  bsz_len = sizeof(ibsz);
  ret = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &ibsz, &bsz_len);

  if (ret == 0) {
    bsz_len = sizeof(obsz);
    ret = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &obsz, &bsz_len);

    if (ret == 0) {
      if (rdbuf_size) *rdbuf_size = ibsz;
      if (wrbuf_size) *wrbuf_size = obsz;
      return;
    }
    else if (errno == ENOTSOCK)
      notsock = 1;
  }
  else if (errno == ENOTSOCK)
    notsock = 1;

  if (notsock && fstat(fd, &sbuf) == 0) {
    if (rdbuf_size) *rdbuf_size = sbuf.st_blksize;
    if (wrbuf_size) *wrbuf_size = sbuf.st_blksize;
  }
  else {
    if (rdbuf_size) *rdbuf_size = RCVBUF_SIZE;
    if (wrbuf_size) *wrbuf_size = SNDBUF_SIZE;
  }
}


void
print(const char *name, int rsize, int wsize)
{
  printf("%20s %10d %10d\n", name, rsize, wsize);
}

void
print_socket_buffer(const char *name, int domain, int type)
{
  int fd;
  int rsize, wsize;

  fd = socket(domain, type, 0);
  if (fd == -1) {
    fprintf(stderr, "socket() failed: %s\n", strerror(errno));
    return;
  }
  fd_blk_size(fd, &rsize, &wsize);
  print(name, rsize, wsize);
  close(fd);
}

void
print_pipe_buffer(const char *name)
{
  int fd[2];
  int rsize, wsize;

  if (pipe(fd) != 0) {
    fprintf(stderr, "pipe() failed: %s\n", strerror(errno));
    return;
  }
  fd_blk_size(fd[0], &rsize, 0);
  fd_blk_size(fd[1], &wsize, 0);
  print(name, rsize, wsize);
  close(fd[0]);
  close(fd[1]);
}


void
print_spair_buffer(const char *name, int domain, int type)
{
  int fd[2];
  int rsize, wsize;

  if (socketpair(domain, type, 0, fd) != 0) {
    fprintf(stderr, "socketpair() failed: %s\n", strerror(errno));
    return;
  }

  fd_blk_size(fd[0], &rsize, &wsize);
  print(name, rsize, wsize);
  close(fd[0]);
  close(fd[1]);
}


void
print_file_buffer(const char *prefix)
{
  char *tmpfile;
  int len, fd, rsize, wsize;

  if (!prefix)
    prefix = "/tmp";

  len = strlen(prefix);
  if (prefix[len - 1] != '/')
    asprintf(&tmpfile, "%s/XXXXXX", prefix);
  else
    asprintf(&tmpfile, "%sXXXXXX", prefix);

  fd = mkstemp(tmpfile);
  unlink(tmpfile);
  fd_blk_size(fd, &rsize, &wsize);

  print(tmpfile, rsize, wsize);

  free(tmpfile);
  close(fd);
}

int
main(int argc, char *argv[])
{
  print_pipe_buffer("PIPE");
  print_spair_buffer("SPAIR-STREAM", AF_LOCAL, SOCK_STREAM);
  print_spair_buffer("SPAIR-DGRAM", AF_LOCAL, SOCK_DGRAM);
  print_socket_buffer("UNIX-STREAM", AF_LOCAL, SOCK_STREAM);
  print_socket_buffer("UNIX-DGRAM", AF_LOCAL, SOCK_DGRAM);
  print_socket_buffer("INET-STREAM", AF_INET, SOCK_STREAM);
  print_socket_buffer("INET-DGRAM", AF_INET, SOCK_DGRAM);
  print_socket_buffer("INET6-STREAM", AF_INET6, SOCK_STREAM);
  print_socket_buffer("INET6-DGRAM", AF_INET6, SOCK_DGRAM);

  if (argc == 1)
    print_file_buffer(0);
  else {
    int i;
    for (i = 1; i < argc; i++)
      print_file_buffer(argv[i]);
  }

  return 0;
}
