/* $Id$ */
/* xmlparse - easy expat wrapper
 * Copyright (C) 2008, 2009  Seong-Kook Shin <cinsky@gmail.com>
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

#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <setjmp.h>

#include "xmlparse.h"
#include "obsutil.h"

#define MIN(a, b)       (((a) < (b)) ? (a) : (b))

struct xmlelement_ {
  char *ns;
  char *name;

  char lev;
  char grab;

  char *text;

  void *data;

  XMLELEMENT *parent;
};


int xp_node_equal(XMLELEMENT *elem, const char *espec);

static char *basename(const char *filename);
static void elm_push(XMLCONTEXT *context, const char *name);
static void elm_pop(XMLCONTEXT *context);
static void start_handler(void *data, const char *name, const char **attrs);
static void end_handler(void *data, const char *name);
static void char_handler(void *data, const char *s, int len);


void
xp_cancel(XMLCONTEXT *context, int val)
{
  siglongjmp(context->jmp, val);
}


unsigned
xp_ignore(XMLCONTEXT *context, int lev)
{
  unsigned old = context->ignore_lev;
  context->ignore_lev = (unsigned)lev;
  return old;
}


void
xp_set_start_handler(XMLCONTEXT *context, xp_start_handler callback)
{
  context->cb_start = callback;
}

void
xp_set_end_handler(XMLCONTEXT *context, xp_end_handler callback)
{
  context->cb_end = callback;
}


void
xp_set_char_handler(XMLCONTEXT *context, xp_char_handler callback)
{
  context->cb_char = callback;
}


void
xp_set_user_data(XMLCONTEXT *context, void *data)
{
  context->data = data;
}


void *
xp_get_user_data(XMLCONTEXT *context)
{
  return context->data;
}


static char *
basename(const char *filename)
{
  char *base;

  base = strrchr(filename, '/');
  if (base)
    return base + 1;

  return (char *)filename;
}


static void
elm_push(XMLCONTEXT *context, const char *name)
{
  XMLELEMENT *p;
  char *q;

  p = OBSTACK_ALLOC(context->pool, sizeof(*p));

  q = strchr(name, XML_NS_SEP);
  if (!q) {
    p->ns = NULL;
    p->name = OBSTACK_STR_COPY(context->pool, name);
  }
  else {
    p->ns = OBSTACK_COPY0(context->pool, name, q - name);
    p->name = OBSTACK_STR_COPY(context->pool, q + 1);
  }
  p->lev = context->lev;
  p->grab = 0;

  p->data = NULL;
  p->text = NULL;

  p->parent = context->stack;

  context->stack = p;
  context->lev++;
}


static void
elm_pop(XMLCONTEXT *context)
{
  XMLELEMENT *parent;

  if (!context->stack)
    return;

  parent = context->stack->parent;
  OBSTACK_FREE(context->pool, context->stack);
  context->stack = parent;
  context->lev--;
}


static void
start_handler(void *data, const char *name, const char **attrs)
{
  XMLCONTEXT *context = (XMLCONTEXT *)data;

  elm_push(context, name);

  if (context->lev <= context->ignore_lev) {
    if (context->cb_start)
      context->cb_start(context, name, attrs);
    xp_ignore(context, -1);
  }
}


static void
end_handler(void *data, const char *name)
{
  XMLCONTEXT *context = (XMLCONTEXT *)data;

  if (context->stack->grab) {
    OBSTACK_1GROW(context->tpool, '\0');
    context->stack->text = OBSTACK_FINISH(context->tpool);
  }

  if (context->cb_end && context->lev <= context->ignore_lev)
    context->cb_end(context, name, context->stack->text);

  if (context->stack->grab)
    OBSTACK_FREE(context->tpool, context->stack->text);

  elm_pop(context);
}


static void
char_handler(void *data, const char *s, int len)
{
  XMLCONTEXT *context = (XMLCONTEXT *)data;

  assert(context->stack != NULL);

  if (context->stack->grab) {
    //printf("grabbing %d bytes\n", len);
    OBSTACK_GROW(context->tpool, s, len);
  }
#if 0
  if (context->cb_char && context->lev <= context->ignore_lev)
    context->cb_char(context, s, len);
#endif  /* 0 */
}


void
xp_dump_stack(XMLCONTEXT *context, FILE *stream)
{
  XMLELEMENT *node;
  //int lev;

  if (!stream)
    stream = stderr;

  //lev = context->lev;
  for (node = context->stack; node != NULL; node = node->parent) {
    fprintf(stream, "#%d  %s (%s)\n", node->lev, node->name,
            node->ns ? node->ns : "*NO-NS*");
  }
}


int
xp_node_equal(XMLELEMENT *elem, const char *espec)
{
  const char *name, *ns;
  char *p;
  int len1, len2;

  if (espec == NULL && elem == NULL)
    return 1;

  if (espec == NULL || elem == NULL)
    return 0;

  p = strchr(espec, XML_NS_SEP);
  if (p) {
    ns = espec;
    name = p + 1;
  }
  else {
    ns = NULL;
    name = espec;
  }

  if (strcmp(name, elem->name) != 0)
    return 0;

  if ((elem->ns != NULL && ns == NULL) ||
      (elem->ns == NULL && ns != NULL))
    return 0;

  if (elem->ns == NULL && ns == NULL)
    return 1;

  len1 = strlen(elem->ns);
  len2 = name - ns - 1;

  /*
   * When the difference between namespace strings is the only ''/'
   * character at the end of one of it, this function assumes that
   * both strings are equal.
   */
  if (abs(len1 - len2) <= 1)
    return (memcmp(elem->ns, ns, MIN(len1, len2)) == 0);

  return 0;
}


void
xp_grab_string(XMLCONTEXT *context, int grab)
{
  context->stack->grab = grab;
}


int
xp_set_stack_data(XMLCONTEXT *context, void *data, size_t size)
{
  void *ptr;

  ptr = OBSTACK_COPY(context->pool, data, size);
  if (ptr < 0)
    return -1;

  context->stack->data = ptr;
  return 0;
}


int
xp_set_stack_data_in_string(XMLCONTEXT *context, const char *s)
{
  void *ptr;

  ptr = OBSTACK_STR_COPY(context->pool, s);
  if (ptr < 0)
    return -1;

  context->stack->data = ptr;
  return 0;
}


void *
xp_get_stack_data(XMLCONTEXT *context, const char *espec)
{
  XMLELEMENT *elem;

  for (elem = context->stack; elem != NULL; elem = elem->parent) {
    if (xp_node_equal(elem, espec)) {
      return elem->data;
    }
  }
  return NULL;
}


int
xp_equal(XMLCONTEXT *context, int nelem, ...)
{
  va_list ap;
  char *espec;
  XMLELEMENT *elem;

  va_start(ap, nelem);

  elem = context->stack;

  while (nelem > 0) {
    espec = (char *)va_arg(ap, char *);

    if (xp_node_equal(elem, espec) == 0) {
      va_end(ap);
      return 0;
    }

    if (elem == NULL)
      break;

    elem = elem->parent;
    nelem--;
  }
  va_end(ap);

  if (nelem == 0)
    return 1;
  else
    return 0;
}


XMLCONTEXT *
xp_open(const char *pathname)
{
  XMLCONTEXT *p;

  p = malloc(sizeof(*p));
  if (!p)
    return NULL;
  memset(p, 0, sizeof(p));

  p->tpool = &p->tpool_;
  if (OBSTACK_INIT(p->tpool) < 0)
    return NULL;

  p->pool = &p->pool_;
  if (OBSTACK_INIT(p->pool) < 0)
    goto err;

  p->filename = OBSTACK_STR_COPY(p->pool, basename(pathname));
  if (!p->filename)
    goto err;

  p->stack = NULL;
  p->lev = 0;
  p->ignore_lev = (unsigned)-1;

  p->parser = XML_ParserCreateNS("UTF-8", XML_NS_SEP);
  if (!p->parser)
    goto err;

  XML_SetStartElementHandler(p->parser, start_handler);
  XML_SetEndElementHandler(p->parser, end_handler);
  XML_SetCharacterDataHandler(p->parser, char_handler);
  XML_SetUserData(p->parser, p);

  return p;

 err:
  OBSTACK_FREE(p->pool, NULL);
  free(p);
  return NULL;
}


void
xp_close(XMLCONTEXT *context)
{
  if (context->parser)
    XML_ParserFree(context->parser);

  if (context->pool)
    OBSTACK_FREE(context->pool, NULL);

  if (context->tpool)
    OBSTACK_FREE(context->tpool, NULL);

  free(context);
}


int
xp_perform(XMLCONTEXT *context, int fd)
{
  int readch = 0;
  int ret, jret;
  void *buf;
  struct stat sbuf;
  int bufsize;

  if (fstat(fd, &sbuf) < 0) {
    fprintf(stderr, "error: fstat(2) failed: %s\n", strerror(errno));
    return -1;
  }
  bufsize = sbuf.st_blksize;

  jret = sigsetjmp(context->jmp, 1);
  if (jret) {
    fprintf(stderr, "warning: callback cancels xp_perform().\n");
    return -2;
  }

  while (1) {
    buf = XML_GetBuffer(context->parser, bufsize);
    if (buf == NULL) {
      /* error */
      fprintf(stderr, "error: XML_GetBuffer() failed: out of memory.\n");
      ret = -1;
      break;
    }
    readch = read(fd, buf, bufsize);
    if (readch < 0) {
      /* error */
      fprintf(stderr, "error: read(2) failed: %s\n", strerror(errno));
      ret = -1;
      break;
    }

    ret = XML_ParseBuffer(context->parser, readch, (readch == 0));
    if (ret != XML_STATUS_OK) {
      fprintf(stderr, "%s: %lu: error: %s\n", context->filename,
              XML_GetCurrentLineNumber(context->parser),
              XML_ErrorString(XML_GetErrorCode(context->parser)));
      /* error */
      ret = -1;
      break;
    }

    if (readch == 0) {
      ret = 0;
      break;
    }
  }
  return ret;
}


int
xp_qnamecmp(const char *spec1, const char *spec2)
{
  const char *p, *q;
  int ret;

  p = strchr(spec1, XML_NS_SEP);
  q = strchr(spec2, XML_NS_SEP);

  if (p && q) {
    ret = strcmp(p + 1, q + 1);
    if (ret != 0)
      return ret;
    p--;
    q--;
    while (*p == '/')
      p--;
    while (*q == '/')
      q--;

    if ((p - spec1) == (q - spec2))
      return memcmp(spec1, spec2, (p - spec1 + 1));
    return (p - spec1) - (q - spec2);
  }
  else if (p == NULL && q == NULL)
    return strcmp(spec1, spec2);

  return p - q;
}




const char *
xp_find_attr(const char *attrs[], const char *spec)
{
  int i;

  for (i = 0; attrs[i] != NULL; i += 2) {
    if (xp_qnamecmp(attrs[i], spec) == 0)
      return attrs[i + 1];
  }
  return NULL;
}


#ifdef TEST_XP
static void
start_cb(XMLCONTEXT *context, void *data, const char *name, const char **attrs)
{
  printf("%*s%s (%d)\n", context->lev * 4, " ", name, context->lev);

  if (context->lev == 5) {
    int ret;

    xp_dump_stack(context, NULL);

    xp_cancel(context, 1);

    ret = xp_equal(context, 4,
                   "http://backend.userland.com/creativeCommonsRssModule/|tmp",
                   "http://backend.userland.com/creativeCommonsRssModule/|tmp-set",
                   "item",
                   "channel");
    printf("EQUALITY: %d\n", ret);
  }
}


int
main(int argc, char *argv[])
{
  XMLCONTEXT *context;
  int fd;
  int ret;

  context = xp_open(argv[1]);

  printf("filename: %s\n", context->filename);

  fd = open(argv[1], O_RDONLY);

  xp_set_start_handler(context, start_cb);

  ret = xp_perform(context, fd);
  fprintf(stderr, "xp_perform() returns %d\n", ret);

  xp_close(context);

  return 0;
}
#endif  /* TEST_XP */
