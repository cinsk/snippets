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

#include <assert.h>
#include <stdlib.h>
#include <limits.h>

#include <bitmap.h>


#ifndef INT_BIT
# define INT_BIT        (sizeof(int) * CHAR_BIT)
#endif


bitmap_t *
bitmap_new(size_t nbits, unsigned flags)
{
     bitmap_t *bm;
     int size = (nbits + INT_BIT) / INT_BIT;

     bm = malloc(sizeof(*bm) - sizeof(int) + size);
     if (!bm)
          return 0;

     bm->num_bits = nbits;
     bm->size = size;
     bm->flags = flags & (~BMO_STATIC);
     bm->block = bm->block_;
     return bm;
}


#define BITMAP_SIZE_REQUIRED(bits)      (sizeof(bitmap_t) - sizeof(int) + \
                                         ((bits) + INT_BIT) / INT_BIT)

bitmap_t *
bitmap_new_with_buffer(size_t nbits, unsigned flags, void *src, size_t *len)
{
     bitmap_t *bm;
     int size = (nbits + INT_BIT) / INT_BIT;

     if (len != NULL) {
          if (*len < BITMAP_SIZE_REQUIRED(nbits)) {
               *len = BITMAP_SIZE_REQUIRED(nbits);
               return NULL;
          }
     }

     bm = (bitmap_t *)src;
     bm->num_bits = nbits;
     bm->size = size;
     bm->flags = flags | BMO_STATIC;
     bm->block = bm->block_;
     return bm;
}


void
bitmap_delete(bitmap_t *bitmap)
{
     free(bitmap);
}


int
bitmap_get_size(bitmap_t *bitmap)
{
     return bitmap->num_bits;
}


bitmap_t *
bitmap_set_size(bitmap_t *bitmap, size_t nbits)
{
     size_t size;
     int i;

     if (bitmap->num_bits == nbits)
          return 0;

     size = (nbits + INT_BIT) / INT_BIT;
     bitmap = realloc(bitmap, sizeof(*bitmap) - sizeof(int) + size);
     if (!bitmap)
          return 0;

     bitmap->block = bitmap->block_;
     bitmap->size = size;

     if (bitmap->num_bits < nbits) {
          bitmap->num_bits = nbits;
          for (i = bitmap->num_bits; i < nbits; i++)
               bitmap_clear(bitmap, i);
     }
     else
          bitmap->num_bits = nbits;

     return bitmap;
}


void
bitmap_or(bitmap_t *dst, const bitmap_t *src)
{
     int i;
     if (dst->num_bits != src->num_bits)
          return;

     for (i = 0; i < dst->size; i++)
          dst->block[i] |= src->block[i];
}


void
bitmap_xor(bitmap_t *dst, const bitmap_t *src)
{
     int i;
     if (dst->num_bits != src->num_bits)
          return;

     for (i = 0; i < dst->size; i++)
          dst->block[i] ^= src->block[i];
}


void
bitmap_and(bitmap_t *dst, const bitmap_t *src)
{
     int i;
     if (dst->num_bits != src->num_bits)
          return;

     for (i = 0; i < dst->size; i++)
          dst->block[i] &= src->block[i];
}


void
bitmap_not(bitmap_t *bitmap)
{
     int i;
     for (i = 0; i < bitmap->size; i++)
          bitmap->block[i] = ~bitmap->block[i];
}


bitmap_t *
bitmap_copy(bitmap_t *src)
{
     int i;
     bitmap_t *bm = bitmap_new(src->num_bits, 0);
     if (!bm)
          return 0;

     for (i = 0; i < src->size; i++)
          bm->block[i] = src->block[i];

     return bm;
}


char *
bitmap_string(bitmap_t *bitmap, char *buf, size_t len)
{
     int i;
     char *p = buf, *q = buf + len;
     for (i = 0; i < bitmap->num_bits; i++) {
          if (p >= q)
               break;
          *p++ = bitmap_get(bitmap, i) ? '1' : '0';
     }
     if (p < q)
          *p = '\0';
     return buf;
}


#ifdef TEST_BITMAP

#include <stdio.h>


int
main(void)
{
     bitmap_t *bm;
     char s[101];
     int i;

     bm = bitmap_new(36, 0);

     for (i = 0; i < 36; i++) {
          if (i % 2)
               bitmap_set(bm, i);
          else
               bitmap_clear(bm, i);
     }

     printf("%s\n", bitmap_string(bm, s, 48));

     bm = bitmap_set_size(bm, 10);
     printf("%s\n", bitmap_string(bm, s, 48));

     bm = bitmap_set_size(bm, 100);
     printf("%s\n", bitmap_string(bm, s, 101));

     return 0;
}
#endif /* TEST_BITMAP */
