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

#include <ctype.h>

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

  char *cur_section;            /* reserved for parsing functions */
  int errno;

  size_t table_size;
  struct confent *table[1];
};


static size_t get_nearest_prime(size_t n, int op);
static unsigned long string_hash(const char *s);

static int parse(CONF *cf, FILE *fp);

#define CFE_OK          0
#define CFE_ERR         -1
#define CFE_QUOTE       -2
#define CFE_NOKEY       -3
#define CFE_NOVALUE     -4

static int get_pair(char *line, char **key, char **value);
static char *get_quoted(char *line, char quote);

static char *cur_section(CONF *cf);
static int parse_section(CONF *cf, char *line);

static int conf_save_stream(CONF *cf, FILE *fp, const char *headers[]);

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

static int blankline(const char *line);
static int eol(FILE *fp, int ch);
static char *getline(FILE *fp, int lookahead, int *lineno);

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

#define IS_VERBOSE(cf)          ((cf)->flags & CF_VERBOSE)
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

  cf->cur_section = 0;
  cf->errno = 0;

  for (i = 0; i < cf->table_size; i++)
    cf->table[i] = NULL;

  return cf;
}


int
conf_load(CONF *cf, const char *pathname, int size_hint)
{
  FILE *fp;
  char *p;
  int ret;

  assert(cf != NULL);

  p = strdup(pathname);
  if (!p)
    return -1;

  fp = fopen(pathname, "r");
  if (!fp) {
    free(p);
    return -1;
  }

  ret = parse(cf, fp);

  if (ret != 0) {
    cf->errno = ret;
    free(p);
  }
  else {
    if (cf->pathname)
      free(cf->pathname);
    cf->pathname = p;
  }

  fclose(fp);
  return ret;
}


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

  cf->dirty = 1;
  return 0;
}


int
conf_remove(CONF *cf, const char *sect, const char *key)
{
  struct confent *ent = del_entry(cf, sect, key);
  sect_unref(cf, ent->sect);
  delete_entry(cf, ent);

  cf->dirty = 1;
  return 0;
}


int
conf_enum_section(CONF *cf, conf_enum_proc proc, void *data)
{
  struct confent *p;
  int index;

  for (index = 0, p = cf->sections; p != 0; p = p->sibling, index++) {
    if (proc(p->key, 0, 0, index, data) < 0)
      break;
  }

  return index;
}


int
conf_enum(CONF *cf, const char *section, conf_enum_proc proc, void *data)
{
  struct confent *sect, *p;
  int index = 0;

  if (section) {
    sect = find_sect(cf, section);
    if (!sect)
      return -1;

    for (p = sect->sect; p != 0; p = p->sibling) {
      if (proc(sect->key, p->key, p->value, index++, data) < 0)
        break;
    }
  }
  else {
    for (sect = cf->sections; sect != NULL; sect = sect->sibling) {
      for (p = sect->sect; p != NULL; p = p->sibling)
        if (proc(sect->key, p->key, p->value, index++, data) < 0)
          break;
    }
  }
  return index;
}


void
conf_set_dirty(CONF *cf, int dirty)
{
  cf->dirty = dirty;
}


int
conf_save_as(CONF *cf, const char *pathname, const char *headers[])
{
  char *p;
  p = strdup(pathname);
  if (!p)
    return -1;

  if (cf->pathname)
    free(cf->pathname);
  cf->pathname = p;

  cf->dirty = 1;

  return conf_save(cf, headers);
}


int
conf_save(CONF *cf, const char *headers[])
{
  FILE *fp;
  int ret;

  if (!cf->dirty)
    return 0;

  if (!cf->pathname)
    return -1;

  fp = fopen(cf->pathname, "w");
  if (!fp)
    return -1;

  ret = conf_save_stream(cf, fp, headers);
  if (ret < 0) {
    fclose(fp);
    return -1;
  }

  fclose(fp);
  return 0;
}


int
conf_section_count(CONF *cf)
{
  return cf->num_sections;
}


int
conf_entry_count(CONF *cf)
{
  return cf->num_entries;
}


static int
conf_save_stream(CONF *cf, FILE *fp, const char *headers[])
{
  struct confent *p, *q;
  int i;

  if (headers)
    for (i = 0; headers[i] != 0; i++)
      fprintf(fp, "# %s\n", headers[i]);

  for (p = cf->sections; p != NULL; p = p->sibling) {
    fprintf(fp, "[%s]\n", p->key);

    for (q = p->sect; q != NULL; q = q->sibling) {
      if (strpbrk(q->key, " \t\b\r\n\v"))
        fprintf(fp, "\"%s\"", q->key);
      else
        fprintf(fp, "%s", q->key);

      fprintf(fp, " = ");

      if (strpbrk(q->value, " \t\b\r\n\v"))
        fprintf(fp, "\"%s\"", q->value);
      else
        fprintf(fp, "%s", q->value);

      fprintf(fp, "\n");
    }
    fprintf(fp, "\n");
  }

  cf->dirty = 0;

  return 0;
}


static int
parse(CONF *cf, FILE *fp)
{
  int ch, ret = -1;
  unsigned lineno = 0;
  char *line;
  char *key, *value;
  int len;

  ch = fgetc(fp);
  if (ch == EOF)
    goto err;

  if ((ret = eatup_bom(fp, ch)) == BOM_INVALID)
    goto err;

  line = getline(fp, (ret == BOM_NONE) ? ch : EOF, &lineno);
  if (line) {
    do {
      /* TODO: parse */
      // printf("%u: %s\n", lineno, line);

      len = strlen(line);
      if (len == 0 || blankline(line)) /* ignore an empty or a blank line */
        continue;

      if (line[0] == '#' || line[0] == '!' || line[0] == ';')
        /* ignore a comment line */
        continue;

      if (parse_section(cf, line) == 0)
        continue;

      ret = get_pair(line, &key, &value);
      if (ret < 0) {
        if (IS_VERBOSE(cf))
          ;
        goto err;
        free(line);
        return ret;
      }

      printf("[%s] %s = %s\n", cur_section(cf), key, value);
      conf_add(cf, cur_section(cf), key, value);

      free(line);
    } while ((line = getline(fp, EOF, &lineno)) != NULL);
  }
  return 0;

 err:
  return ret;
}


static char *
cur_section(CONF *cf)
{
  return cf->cur_section;
}


static int
parse_section(CONF *cf, char *line)
{
  char *p, *q;
  size_t skipped;

  p = line;
  skipped = strspn(p, " \v\t\n\r\b");
  p += skipped;

  if (*p != '[')
    return -1;

  p++;                /* P points the beginning of the section name */

  skipped = strcspn(p, "]");
  q = p + skipped;
  *q = '\0';

  q = strdup(p);
  if (!q)
    return -1;

  if (cf->cur_section)
    free(cf->cur_section);
  cf->cur_section = q;

  return 0;
}


static int
get_pair(char *line, char **key, char **value)
{
  int ret = -1;
  char *p, *q, *r;
  size_t ignored, skipped;

  p = line;

  ignored = strspn(p, " \v\t\n\r\b");
  if (ignored > 0)
    p += ignored;

  if (*p == '"') {
    q = get_quoted(p + 1, *p);
    if (!q) {
      ret = CFE_QUOTE;
      goto end;
    }
    p++;
  }
  else {
    skipped = strcspn(p, " =\v\t\n\r\n");
    if (skipped == 0) {
      ret = CFE_NOKEY;
      goto end;
    }
    *(p + skipped) = '\0';
    q = p + skipped + 1;
  }
  /* Q points the next token char, P points the quoted word */

  ignored = strspn(q, "= \v\t\n\r\b");
  if (ignored > 0)
    q += ignored;

  if (*q == '"') {
    r = get_quoted(q + 1, *q);
    if (!r) {
      ret = CFE_QUOTE;
      goto end;
    }
    q++;
  }
  else {
    skipped = strcspn(q, " \v\t\n\r\b");
    if (skipped == 0) {
      ret = CFE_NOVALUE;
      goto end;
    }
    *(q + skipped) = '\0';
  }

  if (key)
    *key = p;
  if (value)
    *value = q;
  ret = 0;

 end:
  return ret;
}


static char *
get_quoted(char *line, char quote)
{
  char *p = line;

  while (*p != '\0') {
    if (*p == '\\' && *(p + 1) == quote)
      p++;
    else if (*p == quote) {
      *p = '\0';
      return p + 1;
    }
    p++;
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

    if (sect) {
      if (strcmp(p->sect->key, sect) == 0 && strcmp(p->key, key) == 0) {
        if (prev && p != q)
          *prev = q;
        return p;
      }
      return NULL;
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
  for (p = prev = ent->sect->sect; p != NULL && p != ent; p = p->sibling) {
    prev = p;
  }
  /* TODO: Can P be NULL?  */
  if (p == ent->sect->sect)
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
blankline(const char *line)
{
  int len = strlen(line);
  int i;
  for (i = 0; i < len; i++) {
    if (!isspace(line[i]))
      return 0;
  }
  return 1;
}


static int
eol(FILE *fp, int ch)
{
  int lookahead = 0;

  switch (ch) {
  case EOF:
    return 1;

  case '\n':
    return 1;

  case '\r':
    lookahead = fgetc(fp);
    if (lookahead == EOF || lookahead == '\n')
      return 1;
    ungetc(lookahead, fp);
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
getline(FILE *fp, int lookahead, int *lineno)
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
      if (lineno) (*lineno)++;
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
        if (lineno) (*lineno)++;
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
        printf("\t\t%s = %s\n", q->key, q->value);
    }
}
#endif  /* NDEBUG */


#ifdef TEST_CONF
int
enum_proc(const char *sect, const char *key, const char *val,
          int index, void *data)
{
  printf("%d: [%s] %s = %s\n", index, sect, key, val);
  return 0;
}


static const char *headers[] = {
  "",
  "This is automatically generated by conf module.",
  "",
  0,
};

int
main(int argc, char *argv[])
{
  CONF *cf;
  cf = conf_new(64);

  if (argc == 2) {
    //conf_dump(cf, stdout);
    if (conf_load(cf, argv[1], 32) < 0) {
      fprintf(stderr, "conf_load() failed");
    }
  }
#if 1
  puts("\n\n-- enum_section -----------------------------------------");
  conf_enum_section(cf, enum_proc, 0);

  puts("\n\n-- enum_all --------------------------------------------");
  conf_enum(cf, 0, enum_proc, 0);
#endif

  puts("\n\n-- save begin ----------------");
  conf_save_stream(cf, stdout, headers);
  puts("-- save end ----------------");
#if 0
  conf_dump(cf, stdout);
  conf_add(cf, "PANEL", "SIZE","LARGE");
  conf_dump(cf, stdout);
  conf_add(cf, "PANEL", "LEFT","200");
  conf_add(cf, "PANEL", "TOP","100");
  conf_add(cf, "PANEL", "WIDTH","640");
  conf_add(cf, "PANEL", "HEIGHT","480");

  conf_add(cf, "SPLASH", "DURATION", "30");

  conf_dump(cf, stdout);

  conf_remove(cf, "PANEL", "TOP");
#endif  /* 0 */
  conf_dump(cf, stdout);
  conf_close(cf);

  return 0;
}
#endif  /* TEST_CONF */
