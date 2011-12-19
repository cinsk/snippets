/* dlog: message logger for debugging
 * Copyright (C) 2009  Seong-Kook Shin <cinsky@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef DLOG_H_
#define DLOG_H_

#include <stdio.h>

#if 0
#ifndef NDEBUG
# if defined(__STDC__)
#  define dlog(...)       derror(0, __VA_ARGS__)
# elif defined(__GNUC__)
#  define dlog(args...)   derror(0, args)
# else
#  error cannot handle variadic macros
# endif
#else   /* NDEBUG */
# if defined(__STDC__)
#  define dlog(...)       ((void)0)
# elif defined(__GNUC__)
#  define dlog(args...)   ((void)0)
# else
#  error cannot handle variadic macros
# endif
# if !defined(DLOG_BUILD)
#  define dlog_set_output(fp)    ((void)0)
#  define dlog_set_filter(mask)  ((void)0)
#  define dlog_init()            ((void)0)
#  define dlog_thread_init()     ((void)0)
# endif
#endif  /* NDEBUG */
#endif  /* 0 */


#if defined(__STDC__)
#  define dlog(...)       derror(0, __VA_ARGS__)
#elif defined(__GNUC__)
#  define dlog(args...)   derror(0, args)
#else
#  error cannot handle variadic macros
#endif

extern void derror(int status, int errnum, unsigned category,
                   const char *format, ...)
  __attribute__ ((format (printf, 4, 5)));

extern FILE *dlog_set_stream(FILE *fp);
extern unsigned dlog_set_filter(unsigned mask);

extern int dlog_init(void);
extern int dlog_thread_init(void);
extern int dlog_set_thread_name(const char *name);

#define D_LOG           0x00000001
#define D_WARN          0x00000002
#define D_ERR           0x00000004
#define D_THREAD        0x00000008
#define D_SYSCALL       0x00000010

#endif  /* DLOG_H_ */
