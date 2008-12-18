/* GNU fmt module -- simple text formatter.
   Copyright (C) 1994-2006 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Originally written by Ross Paterson <rap@doc.ic.ac.uk>.
 * C module by Seong-Kook Shin <cinsky@gmail.com>. */
#ifndef fmt_h__
#define fmt_h__

/* $Id$ */

/* Flags for fmt_new()
 *
 * FF_MALLOC -- If provided, the return value and the strings from
 *              fmt_format() are allocated using malloc().  Otherwise,
 *              the memory is handled by fmt_format(); The previous
 *              memory is automatically freed by fmt_format().
 */
#define FF_MALLOC       0x01

struct fmt_;
typedef struct fmt_ fmt_t;

extern fmt_t *fmt_new(unsigned flags);
extern void fmt_delete(fmt_t *fmt);
extern void fmt_set_width(fmt_t *p, int width);
extern char **fmt_format (fmt_t *f, const char *s);

#endif  /* fmt_h__ */
