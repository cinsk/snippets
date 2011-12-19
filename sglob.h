/* sglob -- another implementation of glob(3)
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


#ifndef SGLOB_H__
#define SGLOB_H__

#ifndef BEGIN_C_DECLS
# ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
# else
#  define BEGIN_C_DECLS
#  define END_C_DECLS
# endif
#endif /* BEGIN_C_DECLS */

#include <sys/stat.h>

BEGIN_C_DECLS

#define SGLOB_MASK      0x0001

struct sglob_ {
  size_t pathc;
  char **pathv;
  mode_t mask;                  /* specify the exact file type to get */

  size_t capacity_;             /* reserved for internal use */
};
typedef struct sglob_ sglob_t;

/*
 * sglob_() -- collect all matching filenames
 *
 * PATTERN should be a shell-like wildcards.  If PATTERN is NULL, "*"
 * is assumed.  You don't need to initialize the GLOB buffer unless
 * speicified.
 *
 * sglob() returns zero on success (even if no matching file found), or
 * returns -1 on error.
 *
 * If sglob() succeeded, the number of collected files is stored in
 * 'pathc' member, and the actual filenames are stored in 'pathv'
 * member.  Once you finished to use sglob(), you need to call
 * sglobfree() to free all resources allocated by sglob().
 *
 * sglob() does not change the current working directory of the
 * calling process.
 *
 * Note that sglob() does not follow any sub-directory.  Try glob() if
 * you need recursive operation.
 *
 * FLAGS is one or more(or-ed) of following constants:
 *
 * SGLOB_MASK -- If specified, you need to set the 'mask' member of
 *               'glob'.  sglob() only extract the specified file
 *               type.  If not provided, the 'mask' will be
 *               automatically set to S_IFREG, which represent a
 *               regular type.  Only one file type can be specified.
 */
extern int sglob(const char *pattern, int flags, sglob_t *glob);

/*
 * sglob_() -- collect all matching filenames
 *
 * sglob_ works like sglob() except it accept DIRECTORY parameter.
 * DIRECTORY should be the directory name that the sglob_() will search,
 * whereas PATTERN contains the wildcards.  PATTERN must not contain
 * a directory component.
 */
extern int sglob_(const char *directory, const char *pattern, int flags,
                  sglob_t *glob);

/*
 * Free all resources allocated by sglob().
 */
extern void sglobfree(sglob_t *glob);

END_C_DECLS

#endif  /* SGLOB_H__ */
