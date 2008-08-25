/* $Id$ */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "symtable.h"

struct symtable_ {
  unsigned flags;               /* options */

  struct sframe *frame;         /* struct sframe array[size_frame] for
                                   frame links*/
  size_t size_frame;            /* size of FRAME */
  int depth;                    /* current frame depth (index to FRAME) */

  struct snode **table;         /* bucket table */
  size_t size_table;            /* size of TABLE */

  symtable_free_t free_func;    /* user-provided free function */

  struct obstack _pool;
  struct obstack *pool;
};

struct sframe {
  void *base;                   /* dummy for recording the current
                                   point in the symtable_t::pool */
  const char *name;             /* name of the current frame (optional?) */
  struct snode *link;           /* list of all (key, value) pair for
                                   the current frame */
};

struct snode {
  const char *name;             /* name of the key */

  struct snode *flnk;           /* neighbor of the same frame */
  int frame;                    /* depth of the current frame */

  struct snode *prev;           /* previous ptr of the bucket chain */
  struct snode *next;           /* next ptr of the bucket chain */

  int valid;                    /* if zero, this (key, value) is invalid */

  size_t size_val;              /* size of the VAL */

  void *val;
#if 0
  char val[0];                  /* value of (key, value) pair.
                                 * Should be the last member */
#endif  /* 0 */
};


#define symtable_hash(s)        symtable_hash_from_gcc(s)

static unsigned long symtable_hash_from_glib(const char *s);
static unsigned int symtable_hash_from_gcc(const char *str);
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
symtable_register(symtable_t *st, const char *name, const void *data, int len)
{
  int index;
  struct snode *p;

  symtable_unregister(st, name);

  index = symtable_hash(name) % st->size_table;

  p = OBSTACK_ALLOC(st->pool, sizeof(*p));
  if (!p)
    return -1;
  p->valid = 0;

  p->name = OBSTACK_STR_COPY(st->pool, name);
  if (!p->name) {
    OBSTACK_FREE(st->pool, p);
    return -1;
  }

  if (len < 0)
    len = strlen(data) + 1;

  if (len > 0) {
    p->val = OBSTACK_ALLOC(st->pool, len);
    memcpy(p->val, data, len);
  }
  else
    p->val = NULL;
  p->size_val = len;

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

  p->valid = 1;

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
  if (!p)
    return NULL;

  if (size)
    *size = p->size_val;

  return p->val;
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


int
symtable_current_frame(symtable_t *table)
{
  return table->depth;
}


const char *
symtable_get_frame_name(symtable_t *table, int frame_id)
{
  return table->frame[frame_id].name;
}


/*
 * Substitute all variable form of "${name}" to the value of variable
 * "name" in the symbol table.
 *
 * Note that this function is poorly designed: S should point the
 * growing object in st->pool, which seems useless, since we can
 * generate it anywhere by calling OBSTACK_BASE(st->pool).
 *
 * On success, this function returns zero.  Otherwise returns -1.
 */
static int
symtable_var_substitute(symtable_t *st, char *s)
{
  size_t len;
  char *p, *q;
  char *name, *val;
  int var_found = 0;
  len = strlen(s);

 again:
  for (p = s + len; p >= s; p--) {
    if (*p == '$') {
      if (p > s && *(p - 1) == '\\') {
        continue;
      }

      if (*(p + 1) == '{') {
        var_found = 1;
        q = strchr(p + 1, '}');
        if (!q) {
          /* error: Closing '}' not found */
          return -1;
        }
        *q = '\0';
        name = p + 2;

        val = symtable_lookup(st, name, NULL, 0);

        if (!val) {
          strcpy(p, q + 1);
        }
        else {
          size_t val_len = strlen(val);
          size_t remained = strlen(q + 1);
          if (OBSTACK_BLANK(st->pool, val_len) < 0)
            return -1;
          memmove(p + val_len, q + 1, remained + 1);
          memcpy(p, val, val_len);
        }
      }
    }
  }

  if (var_found) {
    var_found = 0;
    goto again;
  }

  return 0;
}


int
symtable_register_substitute(symtable_t *st, const char *name,
                             const char *data)
{
  struct snode *snptr;
  int size;

  symtable_register(st, name, 0, 0);
  snptr = symtable_lookup_node(st, name, SYMTABLE_OPT_CFRAME);
  if (!snptr) {
    /* huh? */
    return 0;
  }
  snptr->valid = 0;

  size = strlen(data) + 1;
  OBSTACK_GROW(st->pool, data, size);
  if (symtable_var_substitute(st, OBSTACK_BASE(st->pool)) < 0) {
    return -1;
  }

  snptr->val = OBSTACK_FINISH(st->pool);
  snptr->size_val = strlen(snptr->val);

  snptr->valid = 1;

  return 0;
}


char *
symtable_esc_substitute(symtable_t *st, const char *key)
{
  /* TODO: Not Implemented Yet. */
  size_t len;
  size_t size;
  char *p, *q;

  len = strlen(key);
  size = len * 2;
  OBSTACK_BLANK(st->pool, size);
  strcpy(OBSTACK_BASE(st->pool), key);

  for (p = q = OBSTACK_BASE(st->pool); *p != '\0'; p++) {
    switch (*p) {
    case '\\':
      switch (*(p + 1)) {
      case '\n':
        p++;
        break;
      case 'n':
        *q++ = '\n';
        p++;
        break;
      case 'r':
        *q++ = '\r';
        p++;
        break;
      case '$':
        *q++ = '\\';
        *q++ = '$';
        p++;
        break;
      default:
        *q++ = *(p + 1);
        p++;
        break;
      }
      break;
    case '$':
      if (*(p + 1) == '$') {
        int add;
        add = snprintf(0, 0, "%u", (unsigned)getpid());
        OBSTACK_BLANK(st->pool, add);
        sprintf(q, "%u", (unsigned)getpid());
        q += add;
        p++;
      }
      else {
        *q++ = *p;
      }
      break;
    default:
      *q++ = *p;
      break;
    }
  }
  *p++ = '\0';

  return OBSTACK_BASE(st->pool);
}


static unsigned int
symtable_hash_from_gcc(const char *str)
{
  /* Streamed from GCC, hashstr function */
  unsigned len = strlen(str);
  unsigned long n = len;
  unsigned long r = 0;
  const unsigned char *s = (const unsigned char *) str;

  do
    r = r * 67 + (*s++ - 113);
  while (--n);

  return r + len;
}


static unsigned long
symtable_hash_from_glib(const char *s)
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


int
symtable_enumerate(symtable_t *sp, int frame,
                   symtable_enum_t proc, void *data)
{
  int i, ret;
  struct snode *node;
  int count = 0;

  for (i = frame; i <= sp->depth; i++) {
    for (node = sp->frame[i].link; node != NULL; node = node->flnk) {
      ret = proc(node->name, node->val, node->size_val, node->frame, data);
      if (ret < 0)
        return ret;
      count++;
    }
  }
  return count;
}


#if 0
int
symtable_enumerate(symtable_t *sp, symtable_enum_t proc, void *data)
{
  int i;
  int ret;
  struct snode *node;
  int count = 0;

  for (i = 0; i < sp->size_table; i++) {
    for (node = sp->table[i]; node != NULL; node = node->next) {
      ret = proc(node->name, node->val, node->size_val, node->frame, data);
      if (ret < 0)
        return ret;
      count++;
    }
  }
  return count;
}
#endif  /* 0 */


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

#define ENV_MAX 4096

void import_env(symtable_t *sp, char *envp[])
{
  char *p;
  int i;
  char buf[ENV_MAX];

  for (i = 0; envp[i] != NULL; i++) {
    if (strlen(envp[i]) >= ENV_MAX)
      continue;

    strcpy(buf, envp[i]);

    p = strchr(buf, '=');
    *p = '\0';
    symtable_register(sp, buf, p + 1, -1);
  }
}


void my_free(const char *name, void *data, size_t len)
{
  //printf("FREE(%s)\n", name);
}


symtable_t *
table_init(void)
{
  symtable_t *p = symtable_new(32, 4, 0);
  return p;
}


int
enum_proc(const char *name, const void *value, size_t size_value,
          int frame, void *data)
{
  printf("%s[%d] = %s(%d)\n", name, frame, (char *)value, size_value);
  return 0;
}

symtable_t *symbol_table;

symtable_t *
table_init(void)
{
  symtable_t *p = symtable_new(32, 4, 0);
  return p;
}


symtable_t *symbol_table;

int
init_interpreter(int *argc, char **argv[])
{
  symbol_table = table_init();
  return 0;
}

int
init_interpreter(int *argc, char **argv[])
{
  symbol_table = table_init();
  return 0;
}

int
main(int argc, char *argv[], char *envp[])
{
  symtable_t *table;
  char buf[1024];
  int len;
  char *p = "X";

  init_interpreter(&argc, &argv);
  table = symbol_table;
  table->free_func = my_free;

  //symtable_dump(table);
  //symtable_register(table, "name", "Seong-Kook Shi1", strlen("Seong-Kook Shin") + 1);

  if (0) {
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
  }

  {
    import_env(table, envp);

    printf("SHELL=|%s|\n", (char *)symtable_lookup(table, "SHELL", 0, 0));
#if 0
    symtable_register(table, "FOO", "${BAR}", -1);
    symtable_register(table, "BAR", "heeh", -1);
    symtable_register_substitute(table, "TEXT",
                                 "Hello, ${FOO}, from ${BAR}.");
    printf("result: %s\n", (char *)symtable_lookup(table, "TEXT", 0, 0));
#endif  /* 0 */

    symtable_register_substitute(table, argv[1], argv[2]);
    printf("%s=|%s|\n", argv[1],
           (char *)symtable_lookup(table, argv[1], 0, 0));


    //p = symtable_esc_substitute(table, "hello\nwq\\\ner\\$df");
    //symtable_enumerate(table, 0, enum_proc, NULL);
  }

  symtable_delete(table);
  return 0;
}

#endif  /* TEST_SYMTABLE */
