#ifndef WRITEPID_H__
#define WRITEPID_H__

#include <sys/types.h>

/* This indirect writing of extern "C" { ... } makes Emacs happy */
#ifndef BEGIN_C_DECLS
# ifdef __cplusplus
#  define BEGIN_C_DECLS  extern "C" {
#  define END_C_DECLS    }
# else
#  define BEGIN_C_DECLS
#  define END_C_DECLS
# endif
#endif /* BEGIN_C_DECLS */

BEGIN_C_DECLS

/*
 * Record PID into the file PATHNAME.
 *
 * If PID is -1, writepid() uses the current process ID.  If any of
 * the directory component in PATHNAME does not exist, writepid() will
 * create it, like 'mkdir -p' command.
 *
 * writepid() returns zero on success, otherwise returns -1 with errno
 * set.
 */
extern int writepid(const char *pathname, pid_t pid);

END_C_DECLS

#endif  /* WRITEPID_H__ */
