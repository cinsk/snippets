/* $Id$ */
/*
 * bitmap manipulation module
 * Copyright (C) 2003  Seong-Kook Shin <cinsky@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef BITMAP_H_
#define BITMAP_H_

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <limits.h>

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

#ifndef INT_BIT
# define INT_BIT        (sizeof(int) * CHAR_BIT)
#endif

struct bitmap_ {
  size_t size;
  size_t num_bits;
  int flags;
  unsigned int *block;
  unsigned int block_[1];
};
typedef struct bitmap_ bitmap_t;

#define BMO_STATIC      0x01
#define BMO_NONE        0x00

extern bitmap_t *bitmap_new(size_t nbits, unsigned flags);
extern bitmap_t *bitmap_new_with_buffer(size_t nbits, unsigned flags,
                                        void *src, size_t *len);
extern void bitmap_delete(bitmap_t *bitmap);
extern int bitmap_get_size(bitmap_t *bitmap);
extern bitmap_t *bitmap_set_size(bitmap_t *bitmap, size_t nbits);
extern void bitmap_or(bitmap_t *dst, const bitmap_t *src);
extern void bitmap_xor(bitmap_t *dst, const bitmap_t *src);
extern void bitmap_and(bitmap_t *dst, const bitmap_t *src);
extern void bitmap_not(bitmap_t *bitmap);
extern bitmap_t *bitmap_copy(bitmap_t *src);


static __inline__ int
bitmap_get(bitmap_t *bitmap, int index)
{
  if (bitmap->block[index / INT_BIT] & (1 << (index % INT_BIT)))
    return 1;
  return 0;
}


static __inline__ void
bitmap_set(bitmap_t *bitmap, int index)
{
  bitmap->block[index / INT_BIT] |= (1 << (index % INT_BIT));
}


static __inline__ void
bitmap_clear(bitmap_t *bitmap, int index)
{
  bitmap->block[index / INT_BIT] &= ~(1 << (index % INT_BIT));
}


static __inline__ void
bitmap_set_all(bitmap_t *bitmap)
{
  size_t i;
  for (i = 0; i < bitmap->size; i++)
    bitmap->block[i] = ~(unsigned char)0;
}


static __inline__ void
bitmap_clear_all(bitmap_t *bitmap)
{
  size_t i;
  for (i = 0; i < bitmap->size; i++)
    bitmap->block[i] = 0;
}


#undef INT_BIT

END_C_DECLS

#endif /* BITMAP_H_ */
