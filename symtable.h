#ifndef SYMTABLE_H__
#define SYMTABLE_H__

/* $Id$ */

/* symtable: simple symbol table implementation
 * Copyright (C) 2008  Seong-Kook Shin <cinsky@gmail.com>
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

#include <obsutil.h>

#define SYMTABLE_OPT_CFRAME     0x0001

typedef void (*symtable_free_t)(const char *name, void *data, size_t len);

struct symtable_;
typedef struct symtable_ symtable_t;


/* Create new symbol table,
 *
 * Where TABLE_SIZE is the number of entries in the bucket table,
 * and MAX_DEPTH is the number of frames that this symbol table will support,
 * and FLAGS is options.
 */
extern symtable_t *symtable_new(size_t table_size,
                                size_t max_depth, unsigned flags);

/* Register new (NAME, DATA) pair
 *
 * Register DATA with LEN size in the NAME of the current frame in ST.
 * If the current frame already have NAME, the old data will be removed.
 */
extern int symtable_register(symtable_t *st,
                             const char *name, void *data, size_t len);

/* Unregister KEY from the current frame.
 *
 * Remove KEY from the current frame of ST.
 *
 * Note that occupied memory will not be freed until symtable_leave() will
 * called.  However, this function DOES call user-registered free function.
 * See `symtable_free_t'.
 */
extern int symtable_unregister(symtable_t *st, const char *key);

/*
 * Look up the definition of KEY.
 *
 * If SIZE is not NULL, the size of data will be set.
 *
 * If KEY is found, symtable_lookup() returns the pointer to the data,
 * otherwise returns NULL.
 */
extern void *symtable_lookup(symtable_t *st, const char *key, size_t *size,
                             unsigned flags);

/*
 * Enter new frame.
 *
 * Since symtable will not allow to insert duplicated keys, calling
 * symtable_enter() is the only way to insert duplicated key.
 *
 * symtable_enter() returns the depth of the current frame.
 */
extern int symtable_enter(symtable_t *st, const char *name);

/*
 * Leave the current frame.
 *
 * symtable_leave() removes all (key, value) pairs of the current frame.
 *
 * symtable_leave() returns the new frame depth after leaving.  If
 * there's no frame to leave, symtable_leave() returns -1,
 */
extern int symtable_leave(symtable_t *table);


extern void symtable_dump(symtable_t *p);

#endif  /* SYMTABLE_H__ */
