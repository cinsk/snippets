/* $Id$ */
/* dlog: message logger for debugging
 * Copyright (C) 2004  Seong-Kook Shin <cinsky@gmail.com>
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

#ifndef NDEBUG
# if defined(__STDC__)
#  define dlog(...)       dlog_(__VA_ARGS__)
# elif defined(__GNUC__)
#  define dlog(args...)   dlog_(args)
# else
#  error cannot handle variadic macros
# endif
# define dlog_set_output(fp)     dlog_set_output_(fp)
# define dlog_set_filter(mask)   dlog_set_filter_(mask)
#else   /* NDEBUG */
# if defined(__STDC__)
#  define dlog(...)       ((void)0)
# elif defined(__GNUC__)
#  define dlog(args...)   ((void)0)
# else
#  error cannot handle variadic macros
# define dlog_set_output(fp)    ((void)0)
# define dlog_set_filter(mask)  ((void)0)
# endif
#endif  /* NDEBUG */

#ifndef NDEBUG
extern FILE *dlog_set_output_(FILE *fp);
extern unsigned dlog_set_filter_(unsigned mask);
extern void dlog_(int ecode, int status, unsigned category,
                  const char *format, ...);
#endif  /* NDEBUG */

extern void derror(int ecode, int status, const char *format, ...);

#define D_LOG           0x00000001
#define D_WARN          0x00000002
#define D_ERR           0x00000004
#define D_THREAD        0x00000008
#define D_SYSCALL       0x00000010

#endif  /* DLOG_H_ */
