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
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <obstack.h>

#include "conf.h"

#ifndef obstack_chunk_alloc
#define obstack_chunk_alloc     malloc
#define obstack_chunk_free      free
#endif

#define CF_ENTRY        1
#define CF_SECT         2

struct confent {
  int type;

  char *key;
  char *value;

  struct confent *next;         /* pointer to the next node in hash chain */
  struct confent *sect;
  struct confent *sibling;
};


struct conf_ {
  char *pathname;

  struct confent *sections;
  int dirty;
  unsigned flags;

  size_t num_entries;
  size_t num_sections;

  size_t table_size;
  struct confent *table[1];
};


static size_t get_nearest_prime(size_t n, int op);
static unsigned long string_hash(const char *s);

static int parse(CONF *cf, FILE *fp);

static struct confent *find_sect(CONF *cf, const char *sect);
static struct confent *find_sect_create(CONF *cf, const char *sect);
static __inline__ void sect_ref(CONF *cf, struct confent *ent);
static __inline__ void sect_unref(CONF *cf, struct confent *ent);

static void add_sectlist(CONF *cf, struct confent *ent);
static struct confent *del_sectlist(CONF *cf, struct confent *ent);

static struct confent *find_entry(CONF *cf, const char *sect, const char *key,
                                  struct confent **prev);
static struct confent *add_entry(CONF *cf, struct confent *sect,
                                 const char *key, const char *val);
static struct confent *del_entry(CONF *cf, const char *sect, const char *key);

static struct confent *new_entry(int type, const char *key, const char *value);
static void delete_entry(CONF *cf, struct confent *ent);

static void add_hash(CONF *cf, int index, struct confent *ent);

static int eol(FILE *fp, int ch);
static char *getline(FILE *fp, int lookahead);

#define BOM_INVALID             (-1)
#define BOM_NONE                0
#define BOM_UTF32_BIG           1
#define BOM_UTF32_LITTLE        2
#define BOM_UTF16_BIG           3
#define BOM_UTF16_LITTLE        4
#define BOM_UTF8                5
static int eatup_bom(FILE *fp, int ch);
static int eatup_chars(FILE *fp, int nchar, ...);

static struct obstack *gl_stack;
static struct obstack gl_stack_;

#define IS_OVERWRITE(cf)        ((cf)->flags & CF_OVERWRITE)
#define IS_PRUNE(cf)            ((cf)->flags & CF_PRUNE)

#define GET_HASH(cf, s) (string_hash(s) % (cf)->table_size)


CONF *
conf_new(int size_hint)
{
  CONF *cf;
  int i;

  size_hint = get_nearest_prime(size_hint, 0);
  cf = malloc(sizeof(*cf) + sizeof(struct confent *) * (size_hint - 1));
  if (!cf)
    return NULL;

  cf->pathname = NULL;
  cf->sections = NULL;
  cf->table_size = size_hint;
  cf->dirty = 0;
  cf->flags = 0;

  cf->num_entries = 0;
  cf->num_sections = 0;

  for (i = 0; i < cf->table_size; i++)
    cf->table[i] = NULL;

  return cf;
}


#if 0
CONF *
conf_load(CONF *cf, const char *pathname, int size_hint)
{
  cf = conf_new(size_hint);
  if (!cf)
    return NULL;

  if (parse(cf, pathname) < 0)
    goto failed;

  return cf;

 failed:
  conf_close(cf);
  return NULL;
}
#endif  /* 0 */


int
conf_close(CONF *cf)
{
  /* TODO: release all resources refered by CF. */
  struct confent *p;
  int i;

  if (cf->dirty) {
    /* TODO: save */
  }

  for (i = 0; i < cf->table_size; i++) {
    if (!cf->table[i])
      continue;

    p = cf->table[i];
    while (p) {
      cf->table[i] = p->next;

      /* free P */
      free(p->key);
      if (p->type == CF_ENTRY && p->value) {
        free(p->value);
      }
      free(p);

      p = cf->table[i];
    }
  }

  if (cf->pathname)
    free(cf->pathname);

  free(cf);
  return 0;
}


int
conf_add(CONF *cf, const char *sect, const char *key, const char *value)
{
  struct confent *s;

  s = find_sect_create(cf, sect);
  if (!s)
    return -1;

  if (add_entry(cf, s, key, value) == 0)
    return -1;

  return 0;
}


int
conf_remove(CONF *cf, const char *sect, const char *key)
{
  struct confent *ent = del_entry(cf, sect, key);
  sect_unref(cf, ent->sect);
  delete_entry(cf, ent);

  return 0;
}


int
parse(CONF *cf, FILE *fp)
{
  int ch, ret;
  char *line;

  ch = fgetc(fp);
  if (ch == EOF)
    err;

  if ((ret = eatup_bom(fp, ch)) == BOM_INVALID)
    err;

  line = getline(fp, (ret == BOM_NONE) ? ch : EOF);
  if (line) {
    do {
      /* TODO: parse */

       free(line);
    } while ((line = getline(fp, EOF)) != NULL);
  }

  return 0;
}


static struct confent *
find_sect(CONF *cf, const char *sect)
{
  struct confent *p;
  unsigned index = GET_HASH(cf, sect);

  for (p = cf->table[index]; p != NULL; p = p->next)
    if (p->type == CF_SECT && strcmp(p->key, sect) == 0)
      return p;

  return NULL;
}


static struct confent *
find_sect_create(CONF *cf, const char *sect)
{
  struct confent *ent;
  unsigned index = GET_HASH(cf, sect);

  ent = find_sect(cf, sect);
  if (ent)
    return ent;

  ent = new_entry(CF_SECT, sect, 0);
  if (!ent)
    return NULL;

  add_hash(cf, index, ent);
  add_sectlist(cf, ent);

  return ent;
}


static void
add_sectlist(CONF *cf, struct confent *ent)
{
  struct confent *p;

  assert(cf);
  assert(ent);
  assert(ent->type == CF_SECT);

  ent->sect = 0;

  p = cf->sections;

  if (!p) {
    cf->sections = ent;
  }
  else {
    for (p = cf->sections; p->sibling != NULL; p = p->sibling)
      ;
    ent->sibling = p->sibling;
    p->sibling = ent;
  }
}


/*
 * ENT should be a member of section list in CF.
 */
static struct confent *
del_sectlist(CONF *cf, struct confent *ent)
{
  struct confent *p;

  assert(ent->type == CF_SECT);
  assert((unsigned)ent->value == 0);

  if (cf->sections == ent) {
    cf->sections = ent->sibling;
  }
  else {
    for (p = cf->sections; p->sibling != 0 && p->sibling != ent; p = p->sibling)
      ;
    if (p->sibling == 0)
      return NULL;
    p->sibling = ent->sibling;
  }
  ent->sibling = 0;
  return ent;
}


static struct confent *
new_entry(int type, const char *key, const char *value)
{
  struct confent *p;

  p = malloc(sizeof(*p));
  if (!p)
    return NULL;

  p->key = strdup(key);
  if (!p->key)
    goto err_key;

  if (value) {
    p->value = strdup(value);
    if (!p->value)
      goto err_val;
  }
  else
    p->value = NULL;

  p->next = p->sect = p->sibling = NULL;
  p->type = type;

  return p;

 err_val:
  free(p->key);
 err_key:
  free(p);
  return NULL;
}


static void
delete_entry(CONF *cf, struct confent *ent)
{
  if (ent) {
    if (ent->type == CF_ENTRY) {
      if (ent->value)
        free(ent->value);
      cf->num_entries--;
    }
    else
      cf->num_sections--;

    if(ent->key)
      free(ent->key);

    free(ent);
  }
}


static void
add_hash(CONF *cf, int index, struct confent *ent)
{
  if (ent->type == CF_ENTRY)
    cf->num_entries++;
  else
    cf->num_sections++;

  ent->next = cf->table[index];
  cf->table[index] = ent;
}


/*
 * Return the entry (sect:key) if found.
 * If PREV is not NULL, *PREV is set to the pointer to the previous entry.
 * If PREV is not NULL and there is no previous entry, *PREV is set to NULL.
 */
static struct confent *
find_entry(CONF *cf, const char *sect, const char *key, struct confent **prev)
{
  struct confent *p, *q;
  int index = GET_HASH(cf, key);

  if (prev)
    *prev = 0;

  for (q = p = cf->table[index]; p != NULL; p = p->next) {
    if (p->type == CF_SECT)
      continue;

    if (sect && strcmp(p->sect->key, sect) == 0 && strcmp(p->key, key) == 0) {
      if (prev)
        *prev = q;
      return p;
    }
    else if (strcmp(p->key, key) == 0) {
      if (prev)
        *prev = q;
      return p;
    }
    q = p;
  }
  return NULL;
}


static struct confent *
add_entry(CONF *cf, struct confent *sect, const char *key, const char *val)
{
  struct confent *ent;
  char *p;
  int index = GET_HASH(cf, key);

  ent = find_entry(cf, sect->key, key, 0);

  if (ent) {
    if (!IS_OVERWRITE(cf))
      return NULL;
    p = strdup(val);
    if (!p)
      return NULL;
    free(ent->value);
    ent->value = p;
  }
  else {
    ent = new_entry(CF_ENTRY, key, val);
    if (!ent)
      return NULL;
    ent->sect = sect;
    sect_ref(cf, sect);
    add_hash(cf, index, ent);

    /* Add to the sibling list */
    ent->sibling = sect->sect;
    sect->sect = ent;
  }

  return ent;
}


/*
 * Remove the entry (sect:key) from the hash table, and return it.
 */
static struct confent *
del_entry(CONF *cf, const char *sect, const char *key)
{
  struct confent *prev, *ent, *p;
  int index = GET_HASH(cf, key);

  /* Remove from the hash chain list */
  ent = find_entry(cf, sect, key, &prev);
  if (!ent)
    return NULL;

  if (!prev)
    cf->table[index] = ent->next;
  else
    prev->next = ent->next;
  ent->next = 0;

  /* Remove from the sect sibling list */
  for (p = prev = ent->sect; p != NULL && p != ent; p = p->sibling) {
    prev = p;
  }
  if (p == ent->sect)
    ent->sect = p->sibling;
  else
    prev->sibling = p->sibling;
  p->sibling = 0;

  return ent;
}


static __inline__ void
sect_ref(CONF *cf, struct confent *ent)
{
  assert(ent->type == CF_SECT);
  ent->value = (char *)((unsigned)ent->value + 1);

  /* TODO: overflow check?? */
}


static __inline__ void
sect_unref(CONF *cf, struct confent *ent)
{
  struct confent *p;
  assert(ent->type == CF_SECT);

  ent->value = (char *)((unsigned)ent->value - 1);

  if ((unsigned)ent->value == 0 && IS_PRUNE(cf)) {
    p = del_sectlist(cf, ent);
    delete_entry(cf, p);
  }
}



static unsigned long
string_hash(const char *s)
{
  /* Stealed from GNU glib, g_string_hash function */
  const char *p = s;
  int len = strlen(s);
  unsigned long h = 0;

  while (len--) {
    h = (h << 5) - h + *p;
    p++;
  }

  return h;
}


static const unsigned primes[] = {
  13,
  17,
  31,
  37,
  61,
  67,
  251,
  257,
  509,
  521,
  1021,
  1031,
  2039,
  2053,
  2053,
  4093,
  4099,
  8191,
  8209,
  32719,
  32749,
  32771,
  65519,
  65537,
  524219,
  524287,
  524309,
  33554393,
  33554467,
  1073741789,
  1073741827,
  2147483029,
};


/* If OP is negative, returns a prime number that is less than N.
 * If OP is zero or positive, returns a prime number which is equal to or
 * greater than N. */
static size_t
get_nearest_prime(size_t n, int op)
{
  size_t i, j;

  if (op < 0) {
    for (i = j = 0; i < sizeof(primes) / sizeof(unsigned); i++) {
      if (primes[i] > n)
        break;
      j = i;
    }
    return primes[j];
  }
  for (i = j = 0; i < (int)sizeof(primes) / sizeof(unsigned); i++) {
    if (primes[i] >= n)
      return primes[i];
  }
  return primes[sizeof(primes) / sizeof(unsigned) - 1];
}


static int
eol(FILE *fp, int ch)
{
  int lookahead = 0;

  switch (ch) {
  case EOF:
    return 1;

  case '\n':
    lookahead = fgetc(fp);
    if (lookahead == EOF)
      return 1;

    if (lookahead != '\r')
      ungetc(lookahead, fp);
    return 1;

  case '\r':
    return 1;

  default:
    break;
  }
  return 0;
}


static void
getline_finalizer(void)
{
  obstack_free(gl_stack, 0);
}


/* Similar to fgets(), returns a line from the stream FP.  If you
 * already read one character, pass it as LOOKAHEAD. Otherwise use
 * EOF.  */
static char *
getline(FILE *fp, int lookahead)
{
  int ch, look;
  char *line, *dup;

  if (gl_stack == 0) {
    gl_stack = &gl_stack_;
    obstack_init(gl_stack);
    atexit(getline_finalizer);
  }

  assert(obstack_object_size(gl_stack) == 0);

  if (lookahead != EOF)
    ch = lookahead;
  else
    ch = fgetc(fp);

  while (ch != EOF) {
    if (eol(fp, ch)) {
      obstack_1grow(gl_stack, '\0');
      break;
    }

    if (ch == '\\') {
      look = fgetc(fp);
      if (look == EOF) {
        obstack_1grow(gl_stack, ch);
        break;
      }

      if (eol(fp, look)) {
        ch = fgetc(fp);
        if (ch == EOF)
          break;
      }
      else {
        obstack_1grow(gl_stack, ch);
        ch = look;
      }
    }

    obstack_1grow(gl_stack, ch);
    ch = fgetc(fp);
  }

  if (ch == EOF) {
    if (obstack_object_size(gl_stack) == 0)
      return NULL;
    obstack_1grow(gl_stack, '\0');
  }

  line = obstack_finish(gl_stack);
  dup = strdup(line);
  obstack_free(gl_stack, line);

  return dup;
}


static int
eatup_chars(FILE *fp, int nchar, ...)
{
  va_list ap;
  int i, ch, expected, ret = 0;

  va_start(ap, nchar);

  for (i = 0; i < nchar; i++) {
    expected = (unsigned char)va_arg(ap, int);

    ch = fgetc(fp);
    if (ch == EOF) {
      ret = -1;
      break;
    }

    else if (ch != expected) {
      ungetc(ch, fp);
      ret = -1;
      break;
    }
  }

  va_end(ap);

  return ret;
}


static int
eatup_bom(FILE *fp, int ch)
{
  int look;

  look = fgetc(fp);
  if (look == EOF)
    return BOM_INVALID;

  if (ch == 0x00 && look == 0x00) {
    if (eatup_chars(fp, 2, 0xfe, 0xff) < 0)
      return BOM_INVALID;
    return BOM_UTF32_BIG;
  }
  else if (ch == 0xff && look == 0xfe) {
    if (eatup_chars(fp, 1, 0x00) < 0)
      return BOM_UTF16_LITTLE;
    else {
      if (eatup_chars(fp, 1, 0x00) < 0)
        return BOM_INVALID;
      return BOM_UTF32_LITTLE;
    }
  }
  else if (ch == 0xfe && look == 0xff) {
    return BOM_UTF16_BIG;
  }
  else if (ch == 0xef && look == 0xbb) {
    if (eatup_chars(fp, 1, 0xbf) < 0)
      return BOM_INVALID;
    return BOM_UTF8;
  }
  else {
    ungetc(look, fp);
    return BOM_NONE;
  }
}


#ifndef NDEBUG
void
conf_dump(CONF *cf, FILE *fp)
{
  struct confent *p, *q;

  fprintf(fp, "conf[%p]-----------------------------\n", cf);
  fprintf(fp, "  pathname: %s\n", cf->pathname);
  fprintf(fp, "  dirty: %d\n", cf->dirty);
  fprintf(fp, "  flags: %08x\n", cf->flags);
  fprintf(fp, "  num_entries: %zu\n", cf->num_entries);
  fprintf(fp, "  num_sections: %zu\n", cf->num_sections);
  fprintf(fp, "  table_size: %zu\n", cf->table_size);

  fprintf(fp, "  sections: %p\n", cf->sections);
  if (cf->sections)
    for (p = cf->sections; p != NULL; p = p->sibling) {
      printf("\t%s [%u]\n", p->key, (unsigned)p->value);

      for (q = p->sect; q != NULL; q = q->sibling)
        printf("\t\t%s:%s\n", q->key, q->value);
    }
}
#endif  /* NDEBUG */


#ifdef TEST_CONF
int
main(int argc, char *argv[])
{
  CONF *cf;
  cf = conf_new(64);
  conf_dump(cf, stdout);
  conf_add(cf, "PANEL", "SIZE","LARGE");
  conf_dump(cf, stdout);
  conf_add(cf, "PANEL", "LEFT","200");
  conf_add(cf, "PANEL", "TOP","100");
  conf_add(cf, "PANEL", "WIDTH","640");
  conf_add(cf, "PANEL", "HEIGHT","480");

  conf_add(cf, "SPLASH", "DURATION", "30");

  conf_dump(cf, stdout);

  conf_close(cf);
  return 0;
}
#endif  /* TEST_CONF */
