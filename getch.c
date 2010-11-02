/* $Id$ */

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "getch.h"


int
getch_(int nonblock, int peek)
{
  struct termios oldt, newt;
  int ch;
  int oldf;
  int ifd = fileno(stdin);

#ifdef GETCH_CHECK_TERM
  if (isatty(ifd) != 0) {
    errno = EBADFD;
    return EOF;
  }
#endif  /* GETCH_CHECK_TERM */

  tcgetattr(ifd, &oldt);
  newt = oldt;

  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(ifd, TCSANOW, &newt);

  if (nonblock) {
    oldf = fcntl(ifd, F_GETFL, 0);
    fcntl(ifd, F_SETFL, oldf | O_NONBLOCK);
  }

  ch = getchar();
  tcsetattr(ifd, TCSANOW, &oldt);

  if (nonblock)
    fcntl(ifd, F_SETFL, oldf);

  if (peek && ch != EOF) {
    ungetc(ch, stdin);
  }

  return ch;
}


#ifdef TEST_GETCH
int
main(void)
{
  int ch;

  while (1) {
    ch = getch();
    if (ch != EOF)
      printf("%d\n", ch);
  }
  return 0;
}
#endif  /* TEST_GETCH */
