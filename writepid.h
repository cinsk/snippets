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

#ifndef WRITEPID_MKDIR_TEMPLATE
#define WRITEPID_MKDIR_TEMPLATE "/bin/mkdir -p \"%s\" >&/dev/null"
#endif

/*
 * Record PID into the file PATHNAME.
 *
 * If PID is 0, writepid() uses the current process ID.  If any of
 * the directory component in PATHNAME does not exist, writepid() will
 * create it recursively.
 *
 * Current implementaion utilize system(3) to create the directories.
 * And it assumes that system(3) uses bash(1), and it uses the command
 * line defined in the macro, WRITEPID_MKDIR_TEMPLATE.  If you want to
 * override it, assign new command line in the global variable,
 * writepid_mkdir_template.
 *
 * writepid() returns zero on success, otherwise returns -1 with errno
 * set.
 *
 * If NO_ATEXIT is zero, writepid() automatically register the
 * atexit(3) handler, writepid_unlink().  You may also need this
 * handler function for handling critical signals, such as SIGTERM.
 */
extern int writepid(const char *pathname, pid_t pid, int no_atexit);

/*
 * The default exit handler for atexit(3).
 *
 * This function will call unlink(2) to remove the file passed as 1st
 * argument in writepid().  This function will try to figure out
 * whether the file is the same file that writepid() created.  If the
 * file is different from as it used to be, this function will leave
 * the file, not unlink(2) it.
 */
extern void writepid_unlink(void);

/*
 * If you want to use custom shell command-line that create the
 * directories for the pidfile, assign new command-line in this
 * variable.  You need to define the variable, though.
 */
extern char *writepid_mkdir_template;

END_C_DECLS

#endif  /* WRITEPID_H__ */
