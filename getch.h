#ifndef GETCH_H__
#define GETCH_H__

/* $Id$ */

/*
 * If compiled with GETCH_CHECK_TERM, kbhit(), getch() and getch_()
 * will return EOF if STDIN does not refer to a terminal.
 */
// #define GETCH_CHECK_TERM



/*
 * Read/Peek a character from STDIN in non-canonical mode.
 *
 * If NONBLOCK is nonzero, it modifies STDIN to non-blocking mode.
 * If PEEK is nonzero, it read the character, then call ungetc() to
 * undo its effect.
 *
 * If NONBLOCK is zero, it will return EOF if no data is available.
 */
extern int getch_(int nonblock, int peek);


/*
 * Check if the STDIN has any character input.
 *
 * kbhit() temporarily modify STDIN to nonblocking/noncanonical mode
 * to get a result.
 *
 * Returns the character code if STDIN has any, otherwise returns EOF.
 *
 * Note that this function does not remove the character in the input
 * buffer, but peek the availability.  You need to call a read
 * function (e.g. getchar, fgetc, etc.) to read the character.
 *
 * If compiled with GETCH_CHECK_TERM, kbhit() returns EOF if STDIN
 * does not refer to a terminal.
 */
static __inline__ int
kbhit(void)
{
  return getch_(1, 1);
}


/*
 * Read a character from STDIN in nonblocking/noncanonical mode.
 *
 * getch() waits for a character input, reads it, then returns a
 * character code.
 */
static __inline__ int
getch(void)
{
  return getch_(0, 0);
}


#endif  /* GETCH_H__ */
