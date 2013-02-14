#ifndef KBHIT_H__
#define KBHIT_H__

struct termios;

/*
 * Initialize the current terminal to enable kbhit().
 *
 * if OLDSTATE is non-null, the previous terminal configuration will
 * be stored in it.
 *
 * On success, return zero.  Otherwise -1, and errno is set.
 */
int kbhit_init(struct termios *oldstate);

/*
 * Restore the terminal state to OLDSTATE if OLDSTATE is non-null.
 */
int kbhit_exit(const struct termios *oldstate);

/*
 * Return non-zero if a key pressed.  Otherwise return zero.
 */
int kbhit(void);

#endif  /* KBHIT_H__ */
