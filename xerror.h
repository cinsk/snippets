#ifndef XERROR_H__
#define XERROR_H__

#ifndef __GNUC__
#error GCC is required to use this header
#endif

/*
 * This header provides simple error message printing functions,
 * which is almost duplicated version of error in GLIBC.
 *
 * Works in Linux and MacOS.
 */

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
 * xerror() is the same as error() in GLIBC.
 */
extern void xerror(int status, int code, const char *format, ...)
  __attribute__((format (printf, 3, 4)));

extern void xdebug(int code, const char *format, ...)
  __attribute__((format (printf, 2, 3)));

extern void xmessage(int status, int code, const char *format, va_list ap);

END_C_DECLS

#endif  /* XERROR_H__ */
