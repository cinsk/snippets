/*
 * Simple access to Java Properties file
 * Copyright (C) 2013  Seong-Kook Shin <cinsky@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 */

#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <fnmatch.h>

#ifdef HAVE_UNISTRING
#include <unistr.h>
#endif

#include "xerror.h"
#ifdef USE_XOBS
#include "xobstack.h"
#else
struct xobs {};
#endif
#include "uthash.h"

#include "properties.h"

#ifdef TEST_PROPERTIES
#define xobs_chunk_alloc malloc
#define xobs_chunk_free  free
#endif  /* TEST_PROPERTIES */

#ifndef FALSE
#define FALSE   0
#define TRUE    (!FALSE)
#endif

#ifndef HAVE_UNISTRING
#include <wchar.h>

typedef wchar_t ucs4_t;
#endif


struct property {
  char *key;
  char *value;

  UT_hash_handle hh;
};

struct ifs {
  char *buffer;
  size_t capacity;              /* blksize * 3 */
  char *begin;
  char *end;

  int fd;
  int blksize;
  int eof;
  char *filename;
};


struct lexer {
  struct ifs *is;

  int bol;                      /* TRUE on beginning of the line */

  int parsekey;
  char *filename;
  unsigned lineno;

  char escseq[5];
  char mb[MB_LEN_MAX];

#ifdef USE_XOBS
  struct xobs *pool;
#else
  char *token;
  size_t token_size;
  int token_idx;
#endif
};


struct properties {
  struct property *root;

  struct lexer *lex;

  const char *filename;

#ifdef USE_XOBS
  struct xobs pool_;
#endif
};



enum {
  TK_EOF = -1,
  TK_SEP = 1,
  TK_NAME,
};

static __inline__ void properties_put_(PROPERTIES *props,
                                       const char *key, const char *value,
                                       int copy);

struct ifs *ifs_open(const char *filename);
void ifs_close(struct ifs *p);
ssize_t ifs_getline(char **line, size_t *linecap, struct ifs *s);

static __inline__ int ifs_fill(struct ifs *s);

static int init_lexer(struct xobs *pool, struct lexer *lex,
                      const char *filename);
static void free_lexer(struct lexer *lex);

static char *token_string(struct lexer *lex);
static int get_token(struct lexer *lex);


#ifndef USE_XOBS
static __inline__ void
token_clear(struct lexer *lex)
{
  free(lex->token);
  lex->token_idx = 0;
}


static __inline__ char *
token_finish(struct lexer *lex)
{
  char *p;

  p = malloc(lex->token_idx + 1);
  memcpy(p, lex->token, lex->token_idx);
  p[lex->token_idx] = '\0';

  lex->token_idx = 0;
  return p;
}

static __inline__ int
token_grow(struct lexer *lex, int c)
{
  size_t newsize;
  char *p;

  if (lex->token_idx >= lex->token_size) {
    newsize = lex->token_size + 4096;
    p = realloc(lex->token, newsize);
    if (!p)
      return 0;
    lex->token = p;
    lex->token_size = newsize;
  }

  lex->token[lex->token_idx++] = (unsigned char)c;
  return 1;
}
#endif  /* USE_XOBS */

struct ifs *
ifs_open(const char *filename)
{
  struct ifs *p = NULL;
  struct stat sbuf;

  int saved_errno = 0;

  p = malloc(sizeof(*p));
  if (!p)
    return NULL;

  p->fd = open(filename, O_RDONLY);
  if (p->fd == -1) {
    saved_errno = errno;
    goto err;
  }

  if (fstat(p->fd, &sbuf) == -1) {
    saved_errno = errno;
    close(p->fd);
    goto err;
  }
  p->blksize = sbuf.st_blksize;

  p->buffer = malloc(p->blksize * 3);
  if (!p->buffer) {
    saved_errno = errno;
    close(p->fd);
    goto err;
  }

  p->filename = strdup(filename);
  if (!p->filename) {
    saved_errno = errno;
    free(p->buffer);
    close(p->fd);
    goto err;
  }

  p->begin = p->end = p->buffer + p->blksize;
  p->eof = FALSE;

  return p;

 err:
  if (p)
    free(p);
  if (saved_errno)
    errno = saved_errno;
  return NULL;
}


void
ifs_close(struct ifs *p)
{
  close(p->fd);

  free(p->filename);
  free(p->buffer);
  free(p);
}


static __inline__ int
ifs_fill(struct ifs *s)
{
  ptrdiff_t remaining;
  int readch;

  assert(s != NULL);

  remaining = s->end - s->begin;

  if (remaining < MB_LEN_MAX && !s->eof) {
    memmove(s->buffer + s->blksize, s->begin, remaining);
    s->begin = s->buffer + s->blksize;
    s->end = s->begin + remaining;

    readch = read(s->fd, s->end, s->blksize);
    if (readch > 0) {
      s->end += readch;
    }
    else if (readch == 0)
      s->eof = TRUE;
    else {
      xdebug(errno, "read(2) set non-zero errno");
      return -1;
    }
  }
  return 0;
}


static __inline__ int
ifs_getc(struct ifs *s)
{
  assert(s != NULL);

  ifs_fill(s);

  if (s->begin < s->end) {
    return *s->begin++;
  }
  else
    return EOF;
}


ssize_t
ifs_getline(char **line, size_t *linecap, struct ifs *s)
{
  int ch;
  char *p, *q, *end;
  size_t newsize;
  ssize_t readch = EOF;

  assert(linecap != NULL);
  assert(s != NULL);

  if (*line == NULL) {
    *line = malloc(LINE_MAX);
    if (!*line)
      return -1;
    *linecap = LINE_MAX;
  }

  p = *line;
  end = *line + *linecap;

  while ((ch = ifs_getc(s)) != EOF) {
    readch++;
    if (p >= end - 1) {
      newsize = *linecap * 2;
      q = realloc(*line, newsize);
      if (!q) {
        *p = '\0';
        return -1;
      }
      p = q + (p - *line);
      end = q + newsize;
      *line = q;
      *linecap = newsize;
    }
    *p++ = ch;

    if (ch == '\n')
      break;
  }

  *p = '\0';
  return readch;
}


int
ifs_ungetc(struct ifs *s, char ch)
{
  if (s->begin > s->buffer) {
    s->begin--;
    *s->begin = ch;
    return 0;
  }
  else {
    xdebug(0, "ifs_ungetc() overflows");
    return -1;
  }
}


static int
init_lexer(struct xobs *pool, struct lexer *lex, const char *filename)
{
  lex->is = ifs_open(filename);
#ifdef USE_XOBS
  lex->pool = pool;
  lex->filename = xobs_copy0(lex->pool, filename, strlen(filename));
#else
  lex->filename = strdup(filename);
  lex->token = 0;
  lex->token_size = 0;
  lex->token_idx = 0;
#endif
  lex->lineno = 1;
  return 0;
}


static void
free_lexer(struct lexer *lex)
{
  if (!lex)
    return;
#ifndef USE_XOBS
  token_clear(lex);
  free(lex->filename);
#endif
  ifs_close(lex->is);
  //xobs_free(lex->pool, NULL);
}


char *
token_string(struct lexer *lex)
{
  char *p, *end, *q, *r;

#ifdef USE_XOBS
  p = xobs_base(lex->pool);
  end = p + xobs_object_size(lex->pool);
#else
  p = lex->token;
  end = lex->token + lex->token_idx;
#endif

  for (q = end - 1; q >= p; q--) {
    if (isspace(*q))
      *q = '\0';
    else
      break;
  }

#ifdef USE_XOBS
  r = xobs_finish(lex->pool);
#else
  r = token_finish(lex);
#endif

  return r;
}


static int
parse(PROPERTIES *props)
{
  int token;
  char *key, *value;
  struct lexer *lex = props->lex;

  while (1) {
    key = value = NULL;

    token = get_token(lex);
    if (token != TK_NAME)
      return -1;
    else
      key = token_string(lex);

    token = get_token(lex);
    if (token != TK_SEP)
      return -1;

    token = get_token(lex);
    if (token != TK_NAME)
      return -1;
    else
      value = token_string(lex);

    printf("[%s] = [%s]\n", key, value);

    properties_put_(props, key, value, 0);
  }
  return 0;
}


static int
get_token(struct lexer *lex)
{
  int ch;
  int i;

 begin:
  while ((ch = ifs_getc(lex->is)) != EOF) {
    if (!isblank(ch)) {
      break;
    }
  }
  if (ch == EOF)
    return TK_EOF;

  switch (ch) {
  case '#':
  case '!':                     /* ignore comments */
    while ((ch = ifs_getc(lex->is)) != EOF) {
      if (ch == '\n') {
        lex->lineno++;
        break;
      }
    }
    goto begin;

  case '\r':
  case '\n':
    lex->lineno++;
    goto begin;

  case '=':
  case ':':
    return TK_SEP;

  case EOF:
    return TK_EOF;

  default:                      /* get NAME  */
    ifs_ungetc(lex->is, ch);

    while (1) {
      ch = ifs_getc(lex->is);

      if (ch == '\\') {
        ch = ifs_getc(lex->is);
        switch (ch) {
        case 'u':
        case 'U':
          /* TODO: get unicode character */
          {
            char *endptr;
            ucs4_t uc;
#ifdef HAVE_UNISTRING
            int len;
#endif

            memset(lex->escseq, 0, sizeof(lex->escseq));
            for (i = 0; i < 4; i++) {
              lex->escseq[i] = ifs_getc(lex->is);
              if (lex->escseq[i] == -1) {
                lex->escseq[i] = '\0';
                break;
              }
            }
            uc = strtoul(lex->escseq, &endptr, 16);
            while (*endptr != '\0')
              ifs_ungetc(lex->is, *endptr++);

#ifdef HAVE_UNISTRING
            len = u8_uctomb((uint8_t *)lex->mb, uc, MB_LEN_MAX);
            if (len >= 0) {
              for (i = 0; i < len; i++) {
#ifdef USE_XOBS
                xobs_1grow(lex->pool, lex->mb[i]);
#else
                token_grow(lex, lex->mb[i]);
#endif
              }
            }
            else {
              /* TODO: len == -1 illegal unicode escape sequence */
            }
#else
            xerror(0, 0, "%s:%d: unicode escape sequence is not supported",
                   lex->filename, lex->lineno);
#ifdef USE_XOBS
            xobs_1grow(lex->pool, '?');
#else
            token_grow(lex, '?');
#endif

#endif  /* HAVE_UNISTRING */
          }
          break;
        case '\n':
          lex->lineno++;
          break;                  /* escaping line terminator */

        case ':':
        case '=':
#ifdef USE_XOBS
          xobs_1grow(lex->pool, ch);
#else
          token_grow(lex, ch);
#endif
          break;

        default:
          /* TODO: handle other escape sequences */
          break;
        }
      }
      else {                    /* non-escape sequence */
        switch (ch) {
        case '=':
#if 0
        case ':':
          /* TODO: if the value part contains ':' where the delimiter was '=',
           * gettoken() can't handle this. -- cinsk */
#endif
        case '\n':
#ifdef USE_XOBS
          xobs_1grow(lex->pool, '\0');
#else
          token_grow(lex, '\0');
#endif
          /* NAME part is ready */
          /* TODO: rtrim NAME part */
          ifs_ungetc(lex->is, ch);
          return TK_NAME;

        case EOF:
#ifdef USE_XOBS
          xobs_1grow(lex->pool, '\0');
#else
          token_grow(lex, '\0');
#endif
          return TK_NAME;

        default:
#ifdef USE_XOBS
          xobs_1grow(lex->pool, ch);
#else
          token_grow(lex, ch);
#endif
          break;
        }
      }
    } /* while */
  }   /* switch */
}


PROPERTIES *
properties_load(const char *pathname, PROPERTIES *reuse)
{
  PROPERTIES *p;

  /* REUSE is not fully implemented yet, read the comment in parse(). */
  assert(reuse == NULL);

  if (!reuse) {
    p = malloc(sizeof(*p));
    if (!p)
      return NULL;
#ifdef USE_XOBS
    xobs_init(&p->pool_);
#endif

    p->root = NULL;
    p->filename = NULL;
    p->lex = NULL;
  }
  else
    p = reuse;

  if (pathname) {
#ifdef USE_XOBS
    p->filename = xobs_copy0(&p->pool_, pathname, strlen(pathname));
#else
    p->filename = strdup(pathname);
#endif

    if (p->lex)
      free_lexer(p->lex);

#ifdef USE_XOBS
    p->lex = xobs_alloc(&p->pool_, sizeof(*p->lex));
#else
    p->lex = malloc(sizeof(*p->lex));
#endif

#ifdef USE_XOBS
    init_lexer(&p->pool_, p->lex, pathname);
#else
    init_lexer(0, p->lex, pathname);
#endif

    parse(p);
  }

  return p;
}


void
properties_close(PROPERTIES *props)
{
  struct property *p, *tmp;

  HASH_ITER(hh, props->root, p, tmp) {
    HASH_DEL(props->root, p);
#ifndef USE_XOBS
    free(p->key);
    free(p->value);
    free(p);
#endif
  }

  free_lexer(props->lex);
#ifdef USE_XOBS
  xobs_free(&props->pool_, NULL);
#else
  free((void *)props->filename);
  free(props->lex);
#endif

  free(props);
}


static __inline__ void
properties_put_(PROPERTIES *props, const char *key, const char *value, int copy)
{
  struct property *p;

  HASH_FIND_STR(props->root, key, p);
  if (p) {
    HASH_DEL(props->root, p);
#ifndef USE_XOBS
    free(p->key);
    free(p->value);
    free(p);
#endif
  }

#ifdef USE_XOBS
  p = xobs_alloc(&props->pool_, sizeof(*p));
  /* TODO: it seems okay to use KEY or VALUE directly, not copying */
  p->key = xobs_copy0(&props->pool_, key, strlen(key));
  p->value = xobs_copy0(&props->pool_, value, strlen(value));
#else
  p = malloc(sizeof(*p));
  if (p) {
    if (copy) {
      p->key = strdup(key);
      p->value = strdup(value);
    }
    else {
      p->key = (char *)key;
      p->value = (char *)value;
    }
  }
#endif

  HASH_ADD_KEYPTR(hh, props->root, p->key, strlen(p->key), p);
}


void
properties_put(PROPERTIES *props, const char *key, const char *value)
{
  properties_put_(props, key, value, 1);
}



const char *
properties_get(PROPERTIES *props, const char *key)
{
  struct property *p;

  HASH_FIND_STR(props->root, key, p);

  if (p)
    return p->value;
  else
    return NULL;
}


int
properties_enum(PROPERTIES *props,
                int (*iter)(const char *key, const char *value, void *data),
                const char *pattern,
                void *data)
{
  struct property *p, *tmp;
  int count = 0;

  HASH_ITER(hh, props->root, p, tmp) {
    if (!pattern || (pattern && fnmatch(pattern, p->key, 0) == 0)) {
      if (iter(p->key, p->value, data) == -1)
        break;

      count++;
    }
  }

  return count;
}


#ifdef TEST_PROPERTIES
int
myiter(const char *key, const char *value, void *data)
{
  printf("[%s] = [%s]\n", key, value);
  return 0;
}

int
main(int argc, char *argv[])
{
  PROPERTIES *props;
  int i;

  props = properties_load(argv[1], NULL);

  printf("--\n");

  properties_enum(props, myiter, argv[2], NULL);

  printf("--\n");

  for (i = 2; i < argc; i++) {
    printf("[%s] = |%s|\n", argv[i], properties_get(props, argv[i]));

    properties_put(props, argv[i], "hello");

    printf("[%s] = |%s|\n", argv[i], properties_get(props, argv[i]));

  }

  properties_close(props);

  return 0;
}
#endif  /* TEST_PROPERTIES */
