/*
 * file-related utilities
 * Copyright (C) 2011  Seong-Kook Shin <cinsky@gmail.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */
#ifndef FILEUTIL_H__
#define FILEUTIL_H__

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

char *dirname_inp(char *pathname);

/*
 * Create new directory as "mkdir -p".
 */
int make_directory(const char *pathname);

/*
 * Concatenate two pathname components.
 *
 * If required, this will add the directory separator('/') between
 * DIRNAME and FILENAME.
 *
 * If DIRNAME is NULL, duplicate FILENAME and return it.  If FILENAME
 * is NULL, duplicate DIRNAME and return it.  At least one argument
 * must be non-NULL.
 *
 * The returned value is malloc-ed string, so you should call free(3)
 * when it is no longer necessary.
 */
char *filename_join(const char *dirname, const char *filename);

/* Like basename(1), return the filename without directory component(s).
 * If PATHNAME ends with "/", it returns null-terminated string.
 *
 * The lifetime of the return value depends on PATHNAME.
 */
const char *filename_basename_imm(const char *pathname);

/* Return the extension part of the filename.
 *
 * For example, the return value points "java" if PATHNAME is
 * "hello.java".  It returns null-terminated string if the filename
 * does not have a dot character in PATHNAME.
 *
 * It also returns null-terminated string if the filename starts with
 * dot.  For example, if PATHNAME is ".emacs", the return value points
 * "".  However, if PATHNAME is ".emacs.el", the return value points
 * "el".
 *
 * The lifetime of the return value depends on PATHNAME.
 */
const char *filename_extension_imm(const char *pathname);

/*
 * Remove directory and its files recursively.
 *
 * If FORCE is non-zero, it tries remove the directory even if a
 * directory has no 'x' bit in its mode specification.  (This is a
 * stronger version of "rm -rf".)
 */
int delete_directory(const char *pathname, int force);

END_C_DECLS


#endif /* FILEUTIL_H__ */
