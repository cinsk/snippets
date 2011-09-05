/* pathjoin -- path join functions
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

#ifndef PATHJOIN_H__
#define PATHJOIN_H__

#include <stddef.h>

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

/*
 * Concatenate all pathname components and save it to BUF.
 *
 * BUFSIZE is the size of BUF.
 * The last argument should be NULL.
 *
 * Returns the bytes needed for successful job.  Thus, if the return
 * value is larger than BUFSIZE, it means that the BUF is not large
 * enough.
 *
 * If you just need to know the size of the required buffer, pass NULL
 * for BUF and 0 for BUFSIZE.
 */
extern int path_join(char buf[], size_t bufsize, ...);

/*
 * Concatenate all pathname components and save it to BUF.
 *
 * This function is the same as path_join() except the arguments are
 * in the vector, ARGV.  The last argument of the ARGV must be NULL.
 */
extern int path_join_v(char buf[], size_t bufsize, const char *argv[]);

END_C_DECLS

#endif  /* PATHJOIN_H__ */
