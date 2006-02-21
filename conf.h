/* $Id$ */
/* conf: configuration file manager
 * Copyright (C) 2006  Seong-Kook Shin <cinsky@gmail.com>
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
#ifndef CONF_H__
#define CONF_H__

#include <stddef.h>

#ifndef NDEBUG
# include <stdio.h>
#endif

#ifndef BEGIN_C_DECLS
# ifdef __cplusplus
#  define BEGIN_C_DECLS   extern "C" {
#  define END_C_DECLS      }
# else
#  define BEGIN_C_DECLS
#  define END_C_DECLS
# endif  /* __cplusplus */
#endif  /* BEGIN_C_DECLS */

BEGIN_C_DECLS

#define CF_OVERWRITE    0x0001
#define CF_PRUNE        0x0002

struct conf_;
typedef struct conf_ CONF;

extern CONF *conf_new(int hash_size);
extern CONF *conf_load(CONF *cf, const char *pathname, int hash_size);

//extern CONF *conf_open(const char *pathname, int size_hint);
extern int conf_close(CONF *cf);

extern int conf_add(CONF *cf, const char *sect,
                    const char *key, const char *value);

extern int conf_remove(CONF *cf, const char *sect, const char *key);

#ifndef NDEBUG
static void conf_dump(CONF *cf, FILE *fp);
#endif

END_C_DECLS


#endif  /* CONF_H__ */
