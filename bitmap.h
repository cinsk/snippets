/* $Id$ */

/*
 * Bitmap
 * Copyright (C) 2003, 2004  Seong-Kook Shin <cinsk.shin@samsung.com>
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

extern bitmap_t *bitmap_new(size_t nbits);
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
