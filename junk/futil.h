/* Filename relative utility
 * Copyright (C) 2003, 2004, 2005  Seong-Kook Shin <cinsky@gmail.com>
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
#ifndef FUTIL_H__
#define FUTIL_H__

#include <sys/types.h>
#include <sys/stat.h>

#include <obstack.h>

#ifdef __cplusplus
# ifndef BEGIN_C_DECLS
#  define BEGIN_C_DECLS   extern "C" {
#  define END_C_DECLS     }
# endif
#else
# define BEGIN_C_DECLS
# define END_C_DECLS
#endif  /* __cplusplus */

BEGIN_C_DECLS

struct fu_findfiles_ {
  size_t nelem;
  const char *names[1];
};
typedef struct fu_findfiles_ fu_findfiles_t;

extern int fu_init(int use_atexit);
extern void fu_end(void);


static __inline__ int
fu_isdir_with_stat(const struct stat *sbuf)
{
  return S_ISDIR(sbuf->st_mode);
}


static __inline__ int
fu_islink_with_stat(const struct stat *sbuf)
{
  return S_ISLNK(sbuf->st_mode);
}

extern int fu_isdir(const char *pathname);
extern char *fu_url2pathname(const char *url);
extern int fu_datasync(const char *pathname);

extern long fu_blksize(const char *pathname);

#define FU_NORECURSE    0x0001
#define FU_NOFOLLOW     0x0002
#define FU_DIRPRED      0x0004
#define FU_URLFORM      0x0008

extern fu_findfiles_t *fu_find_files(const char *basedir, struct obstack *pool,
                                     size_t nbulk, int flags,
                                     int (*predicate)(const char *, void *),
                                     void *data);

END_C_DECLS

#endif  /* FUTIL_H__ */
