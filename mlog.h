/* $Id$ */
/*
 * stream/syslog logger module
 * Copyright (C) 2003, 2004  Seong-Kook Shin <cinsky@gmail.com>
 */

#ifndef MLOG_H_
#define MLOG_H_

#include <stdio.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif


/* This indirect writing of extern "C" { ... } makes Emacs happy */
#ifndef BEGIN_C_DECLS
# define BEGIN_C_DECLS  extern "C" {
# define END_C_DECLS    }
#endif  /* BEGIN_C_DECLS */

#ifdef __cplusplus
BEGIN_C_DECLS
#endif


#define MLOG_BUFFER_MAX 4096


/* You should define PROGRAM_NAME somewhere in your code. */
extern const char *program_name;

/* Set the stream for logging.
 * Returns zero on success, otherwise returns -1 */
extern int mlog_set_stream(FILE *fp);

/* Get the stream for logging. */
extern FILE *mlog_get_stream(void);

extern void mlog(const char *format, ...);

#define MLOG(expr, ...)         do { if (expr) mlog(__VA_ARGS__); } while (0)


#ifdef __cplusplus
END_C_DECLS
#endif

#endif /* MLOG_H_ */
