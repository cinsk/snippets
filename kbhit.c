#define _GNU_SOURCE     1

#include <string.h>

#include <termios.h>
#include <unistd.h>
#include <errno.h>

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "kbhit.h"

int
kbhit_init(struct termios *oldstate)
{
  struct termios term;
  int ret, saved_errno;

  ret = tcflow(STDIN_FILENO, TCOOFF);
  if (ret != 0)
    return -1;

  ret = tcflush(STDIN_FILENO, TCIFLUSH);
  if (ret != 0)
    goto err;

  ret = tcgetattr(STDIN_FILENO, &term);
  if (ret != 0)
    goto err;

  if (oldstate)
    memcpy(oldstate, &term, sizeof(term));

  //term.c_iflag &= (~ICANON | ECHO);
  //term.c_iflag &= (~ICANON);
  cfmakeraw(&term);

  term.c_cc[VMIN] = 0;
  term.c_cc[VTIME] = 0;

  ret = tcsetattr(STDIN_FILENO, TCSANOW, &term);
  if (ret != 0)
    goto err;

  ret = tcflow(STDIN_FILENO, TCOON);
  if (ret != 0)
    return -1;

  return 0;

err:
  saved_errno = errno;
  tcflow(STDIN_FILENO, TCOON);
  errno = saved_errno;

  return -1;
}


int
kbhit_exit(const struct termios *oldstate)
{
  int ret;

  if (!oldstate)
    return 0;

  ret = tcflow(STDIN_FILENO, TCOOFF);
  if (ret != 0)
    return -1;

  ret = tcflush(STDIN_FILENO, TCIFLUSH);
  if (ret != 0)
    return -1;

  ret = tcsetattr(STDIN_FILENO, TCSANOW, oldstate);
  if (ret != 0)
    return -1;

  ret = tcflow(STDIN_FILENO, TCOON);
  if (ret != 0)
    return -1;

  return 0;
}


int
kbhit(void)
{
  struct timeval tv;
  fd_set rdfs;

  tv.tv_sec = 0;
  tv.tv_usec = 0;

  FD_ZERO(&rdfs);
  FD_SET(STDIN_FILENO, &rdfs);
  // select ( STDIN_FILENO + 1, &rdfs, NULL, NULL, &tv );
  select (STDIN_FILENO + 1, &rdfs, NULL, NULL, NULL);

  return FD_ISSET(STDIN_FILENO, &rdfs);
}


#ifdef _TEST_KBHIT
#include <stdio.h>

int
main(void)
{
  char buf[100];
  int readch;

  struct termios term;

  kbhit_init(&term);
  fprintf(stderr, "Press any key to continue...\r\n");
  while (!kbhit()) {
  }
  readch = read(STDIN_FILENO, buf, 100);
  kbhit_exit(&term);

  return 0;
}
#endif  /* _TEST_KBHIT */
