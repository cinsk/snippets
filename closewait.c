#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <getopt.h>
#include "xerror.h"

pthread_t child_thread;
int server_port = 19999;
int try_count = -1;
int block_term = 0;

struct option longopts[] = {
  { "count", required_argument, 0, 'c' },
  { "port", required_argument, 0, 'p' },
  { "block", no_argument, 0, 'b' },
  { 0, 0, 0, 0, },
};

static void *child_main(void *arg);

void dummy_handler(int sig) {}

int
main(int argc, char *argv[])
{
  int sfd;
  struct sockaddr_in addr;
  char buf[LINE_MAX];
  int opt;

  while ((opt = getopt_long(argc, argv, "c:p:b", longopts, NULL)) != -1) {
    switch (opt) {
    case 'p':
      server_port = atoi(optarg);
      break;
    case 'c':
      try_count = atoi(optarg);
      break;
    case 'b':
      block_term = 1;
      break;
    default:
      xerror(1, 0, "Usage: %s -p PORT -c COUNT", argv[0]);
    }
  }

  if (block_term) {
    sigset_t set;
    struct sigaction act;

    sigemptyset(&set);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, 0);

    act.sa_flags = 0;
    sigfillset(&act.sa_mask);
    act.sa_handler = dummy_handler;

    sigaction(SIGTERM, &act, 0);
  }


  sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1)
    xerror(1, errno, "socket() failed");

  {
    int value = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) == -1)
      xerror(1, errno, "setsockopt() failed");
  }


  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(server_port);

  if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    xerror(1, errno, "bind() failed");

  if (listen(sfd, 5) < 0)
    xerror(1, errno, "listen() failed");

  {
    int ret = pthread_create(&child_thread, 0, child_main, 0);
    if (ret)
      xerror(0, ret, "pthread_create() failed");
  }

  while (1) {
    int readch;
    int fd = accept(sfd, 0, 0);
    if (fd < 0)
      xerror(0, errno, "accept() failed");

#if 0
    readch = read(fd, buf, LINE_MAX);
    if (readch > 0) {
      write(STDOUT_FILENO, buf, readch);
    }
#endif  /* 0 */
    usleep(1000000);
    close(fd);
  }

  {
    int ret = pthread_join(child_thread, 0);
    if (ret)
      xerror(0, ret, "pthread_join failed");
  }

  close(sfd);

  return 0;
}



static void *
child_main(void *arg)
{
  int fd;
  int i;
  struct sockaddr_in addr;

  usleep(5000000);

  for (i = 0; try_count == -1 || i < try_count; i++) {
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
      xerror(0, errno, "child: socket(2) failed");
      continue;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server_port);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
      xerror(0, errno, "child: connect(2) failed");
      close(fd);
      continue;
    }

    usleep(5000000);
  }

  return 0;
}
