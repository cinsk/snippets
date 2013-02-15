#ifndef KBHIT_H__
#define KBHIT_H__

#include <termios.h>
#include <unistd.h>

/* This indirect using of extern "C" { ... } makes Emacs happy */
#ifndef BEGIN_C_DECLS
# ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
# else
#  define BEGIN_C_DECLS
#  define END_C_DECLS
# endif
#endif /* BEGIN_C_DECLS */

BEGIN_C_DECLS

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

END_C_DECLS

#endif  /* KBHIT_H__ */
