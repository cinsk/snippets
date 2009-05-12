/* $Id$ */
/*
 * snprintx: snprintf-like functions for easy formatting strings
 * Copyright (C) 2009  Seong-Kook Shin <cinsky@gmail.com>
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

#ifndef SNPRINTX__

#include <sys/types.h>

#ifndef __GNUC__
#define ATTR(x)
#else
#define ATTR    __attribute__
#endif

/*
 * snprintx: format string into the buffer
 *
 * Similar to `snprintf', this function formats the given FORMAT string
 * except that it updates the argument BUF and SIZE accordingly.
 *
 * When the buffer is enough large to hold the result, *BUF points the
 * end of the string (e.g. **BUF points the null character), *SIZE is
 * the number of the characters, and it returns zero.
 *
 * When the buffer is not enough, *BUF points the right next to the
 * end of the buffer (sizeof(*BUF) + *SIZE), *SIZE is -1, and it
 * returns the number of characters which whould be generated for the
 * given input, excluding the trailing null.
 */
extern int snprintx(char **buf, ssize_t *size, const char *format, ...)
  ATTR ((format (printf, 3, 4)));

/*
 * This is the equivalent of `snprintx' with the variable argument
 * list specified directly as for `vprintf'.
 */
extern int vsnprintx(char **buf, ssize_t *size, const char *format, va_list ap)
  ATTR ((format (printf, 3, 0)));



#endif  /* SNPRINTX__ */
