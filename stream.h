/* $Id$ */
/*
 * Light file stream
 * Copyright (C) 2004  Seong-Kook Shin <cinsky@gmail.com>
 */
#ifndef STREAM_H_
#define STREAM_H_

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef LINUX
# define HAVE_SYS_TYPES_H
# define HAVE_SYS_STAT_H
# define HAVE_FCNTL_H
# define HAVE_STDIO_H
# define HAVE_ERRNO_H
# define HAVE_UNISTD_H
#endif /* LINUX */

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifdef HAVE_STDIO_H
# include <stdio.h>
#endif

#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdarg.h>
#include <stddef.h>

/*
 * If you're system doesn't support macros such as O_RDONLY, O_TRUNC, etc,
 * you need to define them manually somewhere else. A good starting point
 * is define them in <config.h> and define the macro HAVE_CONFIG_H.
 *
 * If you're not sure what I am talking about, just copy following macro
 * definitions somewhere in your codes.
 */
#if 0
#define O_RDONLY           00
#define O_WRONLY           01
#define O_RDWR             02
#define O_CREAT          0100
#define O_TRUNC         01000
#define O_APPEND        02000

#define _IOFBF          0
#define _IOLBF          1
#define _IONBF          2

#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

#endif /* 0 */

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

struct stream_ops {
  int (*open)(const char *pathname, int flags, void *data);
  int (*creat)(const char *pathname, mode_t mode, void *data);
  int (*close)(int fd, void *data);

  ssize_t (*read)(int fd, void *buf, size_t count, void *data);
  ssize_t (*write)(int fd, const void *buf, size_t count, void *data);
  off_t (*lseek)(int fd, off_t offset, int whence, void *data);
};

struct stream_;
typedef struct stream_ stream_t;


#define STREAM_IONBF    _IONBF  /* no buffered */
#define STREAM_IOLBF    _IOLBF  /* line buffered */
#define STREAM_IOFBF    _IOFBF  /* fully buffered */


extern int stream_errno;

extern stream_t *s_open(const struct stream_ops *ops,
                        const char *pathname, const char *mode,
                        void *data);
extern int s_close(stream_t *s);

extern int s_setvbuf(stream_t *s, char *buf, int mode, size_t size);

extern off_t s_seek(stream_t *s, off_t offset, int whence);
extern int s_getc(stream_t *s);
extern int s_ungetc(stream_t *s, int c);
extern char *s_gets(stream_t *s, char *str, int size);
extern int s_putc(stream_t *s, int ch);
extern int s_puts(stream_t *s, const char *str);
extern size_t s_read(stream_t *s, void *ptr, size_t size, size_t nmemb);
extern size_t s_write(stream_t *s, const void *ptr, size_t size, size_t nmemb);

extern int s_printf(stream_t *s, const char *format, ...);
extern int s_vprintf(stream_t *s, const char *format, va_list ap);

extern int s_scanf(stream_t *s, const char *format, ...);
extern int s_vscanf(stream_t *s, const char *format, va_list ap);

END_C_DECLS

#endif /* STREAM_H_ */
