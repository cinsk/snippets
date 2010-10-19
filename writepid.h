/* writepid: easy function to create .pid file.
 * Copyright (C) 2010  Seong-Kook Shin <cinsky@gmail.com>
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
