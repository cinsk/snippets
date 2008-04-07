/* $Id$ */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "symtable.h"

struct symtable_ {
  unsigned flags;

  struct sframe *frame;
  size_t size_frame;
  int depth;

  struct snode **table;
  size_t size_table;

  symtable_free_t free_func;

  struct obstack _pool;
  struct obstack *pool;
};

struct sframe {
  void *base;
  const char *name;
  struct snode *link;
};

struct snode {
  const char *name;

  struct snode *flnk;
  int frame;

  struct snode *prev;
  struct snode *next;

  int valid;

  size_t size_val;
  char val[0];                  /* should be the last member */
};


static unsigned long symtable_hash(const char *s);
static struct snode *symtable_lookup_node(symtable_t *st,
                                          const char *key, unsigned flags);


symtable_t *
symtable_new(size_t table_size, size_t max_frame, unsigned flags)
{
  symtable_t *p;
  int i;

  p = malloc(sizeof(*p));
  if (!p)
    return NULL;

  p->pool = &p->_pool;
  if (OBSTACK_INIT(p->pool) < 0) {
    free(p);
    return NULL;
  }
  p->frame = OBSTACK_ALLOC(p->pool, sizeof(*p->frame) * max_frame);
  if (!p->frame)
    goto err;
  for (i = 0; i < max_frame; i++) {
    p->frame[i].base = NULL;
    p->frame[i].link = NULL;
  }
  p->size_frame = max_frame;

  p->table = OBSTACK_ALLOC(p->pool, sizeof(*p->table) * table_size);
  if (!p->table)
    goto err;
  for (i = 0; i < table_size; i++)
    p->table[i] = NULL;
  p->size_table = table_size;

  p->flags = flags;
  p->depth = -1;

  symtable_enter(p, "*BASE*");

  p->free_func = NULL;

  return p;

 err:
  OBSTACK_FREE(p->pool, 0);
  free(p);
  return NULL;
}


void
symtable_delete(symtable_t *st)
{
  while (symtable_leave(st) != -1)
    ;

  OBSTACK_FREE(st->pool, NULL);
  free(st);
}


int
symtable_register(symtable_t *st, const char *name, void *data, size_t len)
{
  int index;
  struct snode *p;

  index = symtable_hash(name) % st->size_table;

  p = OBSTACK_ALLOC(st->pool, sizeof(*p) + len);
  if (!p)
    return -1;

  p->name = OBSTACK_STR_COPY(st->pool, name);
  if (!p->name) {
    OBSTACK_FREE(st->pool, p);
    return -1;
  }
  memcpy(p->val, data, len);
  p->size_val = len;
  p->valid = 1;

  p->flnk = st->frame[st->depth].link;
  st->frame[st->depth].link = p;
  p->frame = st->depth;

  p->next = p->prev = NULL;

  if (st->table[index] == NULL) {
    st->table[index] = p;
  }
  else {
    p->prev = st->table[index]->prev;
    st->table[index]->prev = p;
    p->next = st->table[index];
    st->table[index] = p;
  }

  return 0;
}


int
symtable_unregister(symtable_t *st, const char *key)
{
  struct snode *p;
  p = symtable_lookup_node(st, key, SYMTABLE_OPT_CFRAME);

  if (!p)
    return -1;

  if (st->free_func)
    st->free_func(p->name, p->val, p->size_val);

  p->valid = 0;
  return 0;
}


static struct snode *
symtable_lookup_node(symtable_t *st, const char *key, unsigned flags)
{
  int index;
  struct snode *p;

  index = symtable_hash(key) % st->size_table;
  for (p = st->table[index]; p != NULL; p = p->next) {
    if (p->valid && strcmp(key, p->name) == 0) {
      if (flags & SYMTABLE_OPT_CFRAME) {
        if (p->frame == st->depth)
          return p;
        else
          return NULL;
      }
      else
        return p;
    }
  }
  return NULL;
}


void *
symtable_lookup(symtable_t *st, const char *key, size_t *size, unsigned flags)
{
  struct snode *p;

  p = symtable_lookup_node(st, key, flags);

  if (p && size)
    *size = p->size_val;
  return p;
}


/* Enter new frame */
int
symtable_enter(symtable_t *st, const char *name)
{
  if (st->depth > (int)st->size_frame)
    return -1;

  st->depth++;

  st->frame[st->depth].base = OBSTACK_ALLOC(st->pool, 1);
  if (name)
    st->frame[st->depth].name = OBSTACK_STR_COPY(st->pool, name);
  else
    st->frame[st->depth].name = NULL;

  st->frame[st->depth].link = NULL;

  return st->depth;
}


int
symtable_leave(symtable_t *st)
{
  struct snode *p;
  int index = -1;

  if (st->depth < -1)
    return -1;

  if (st->free_func)
    for (p = st->frame[st->depth].link; p != NULL; p = p->flnk) {
      index = symtable_hash(p->name) % st->size_table;

      if (p->valid && st->free_func) {
        st->free_func(p->name, p->val, p->size_val);
        p->valid = 0;
      }

      if (p->prev != NULL)
        p->prev->next = p->next;
      else
        st->table[index] = p->next;

      if (p->next != NULL)
        p->next->prev = p->prev;
    }

  OBSTACK_FREE(st->pool, st->frame[st->depth].base);

  st->depth--;
  return st->depth;
}


static unsigned long
symtable_hash(const char *s)
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


void
symtable_dump(symtable_t *p)
{
  int i;
  struct snode *node;

  printf("symtable[%p]\n", p);
  printf("  flags: 0x%08X\n", p->flags);
  printf("  depth: %d\n", p->depth);
  printf("  frame: size(%u)\n", p->size_frame);

  for (i = 0; i <= p->depth; i++)
    printf("    [%d]: base(%p), link(%p) name(%s)\n", i,
           p->frame[i].base, p->frame[i].link, p->frame[i].name);

  printf("  table: size(%u)\n", p->size_table);
  for (i = 0; i < p->size_table; i++) {
    node = p->table[i];
    if (node) {
      printf("    [%s]", node->name);
      node = node->next;
    }
    else
      printf("    [NIL]");

    for (; node != NULL; node = node->next) {
      printf("->[%s]", node->name);
    }
    putchar('\n');
  }
}


#ifdef TEST_SYMTABLE

void my_free(const char *name, void *data, size_t len)
{
  printf("FREE(%s)\n", name);
}


int
main(int argc, char *argv[])
{
  symtable_t *table;
  char buf[1024];
  int len;
  char *p = "X";

  table = symtable_new(32, 4, 0);
  table->free_func = my_free;

  //symtable_dump(table);
  //symtable_register(table, "name", "Seong-Kook Shi1", strlen("Seong-Kook Shin") + 1);

  while (fgets(buf, 1024, stdin) != 0) {
    if (buf[0] == '\n')
      continue;
    switch (buf[0]) {
    case '@':
      printf("Enter new frame\n");
      symtable_dump(table);
      symtable_enter(table, NULL);
      continue;
    case '~':
      printf("Leave current frame\n");
      symtable_dump(table);
      symtable_leave(table);
      continue;
    default:
      break;
    }

    len = strlen(buf);
    buf[len - 1] = '\0';
    len--;

    printf("Registering name(%s)...\n", buf);
    symtable_register(table, buf, p, 2);
  }

  symtable_dump(table);

  symtable_delete(table);
  return 0;
}

#endif  /* TEST_SYMTABLE */
