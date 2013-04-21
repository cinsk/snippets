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

#ifdef HAVE_UNISTRING
#include <unistr.h>
#endif

#include "xerror.h"
#include "obsutil.h"
#include "uthash.h"

#include "properties.h"

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

  struct obstack *pool;
};


struct properties {
  struct property *root;

  struct lexer *lex;

  const char *filename;

  struct obstack pool_;
};



enum {
  TK_EOF = -1,
  TK_SEP = 1,
  TK_NAME,
};

struct ifs *ifs_open(const char *filename);
void ifs_close(struct ifs *p);
ssize_t ifs_getline(char **line, size_t *linecap, struct ifs *s);

static __inline__ int ifs_fill(struct ifs *s);

static int init_lexer(struct obstack *pool, struct lexer *lex, const char *filename);
static void free_lexer(struct lexer *lex);

static char *token_string(struct lexer *lex);
static int get_token(struct lexer *lex);


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


static int __inline__
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
init_lexer(struct obstack *pool, struct lexer *lex, const char *filename)
{
  lex->is = ifs_open(filename);
  lex->pool = pool;

  lex->filename = obs_copy0(lex->pool, filename, strlen(filename));
  lex->lineno = 1;

  return 0;
}


static void
free_lexer(struct lexer *lex)
{
  ifs_close(lex->is);
  //obs_free(lex->pool, NULL);
}


char *
token_string(struct lexer *lex)
{
  char *p, *end, *q;

  p = obs_base(lex->pool);
  end = p + obs_object_size(lex->pool);

  for (q = end - 1; q >= p; q--) {
    if (isspace(*q))
      *q = '\0';
    else
      break;
  }

  return obs_finish(lex->pool);
}


static int
parse(struct lexer *lex, struct property **props)
{
  int token;
  char *key, *value;
  struct property *p;

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

#if 1
    /* TODO: It should contains logic to delete the key first, like properties_put() */
    //properties_put(props, key, value);
    p = obs_alloc(lex->pool, sizeof(*p));
    if (!p)
      return -1;
    p->key = key;
    p->value = value;

    HASH_ADD_KEYPTR(hh, *props, p->key, strlen(p->key), p);
#endif  /* 0 */
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
              for (i = 0; i < len; i++)
                obs_1grow(lex->pool, lex->mb[i]);
            }
            else {
              /* TODO: len == -1 illegal unicode escape sequence */
            }
#else
            xerror(0, 0, "%s:%d: unicode escape sequence is not supported",
                   lex->filename, lex->lineno);
            obs_1grow(lex->pool, '?');
#endif  /* HAVE_UNISTRING */
          }
          break;
        case '\n':
          lex->lineno++;
          break;                  /* escaping line terminator */

        case ':':
        case '=':
          obs_1grow(lex->pool, ch);
          break;

        default:
          /* TODO: handle other escape sequences */
          break;
        }
      }
      else {                    /* non-escape sequence */
        switch (ch) {
        case '=':
        case ':':
        case '\n':
          obs_1grow(lex->pool, '\0');

          /* NAME part is ready */
          /* TODO: rtrim NAME part */
          ifs_ungetc(lex->is, ch);
          return TK_NAME;

        case EOF:
          obs_1grow(lex->pool, '\0');
          return TK_NAME;

        default:
          obs_1grow(lex->pool, ch);
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
    obs_init(&p->pool_);

    p->root = NULL;
    p->filename = NULL;
    p->lex = NULL;
  }
  else
    p = reuse;

  if (pathname) {
    p->filename = obs_copy0(&p->pool_, pathname, strlen(pathname));

    if (p->lex)
      free_lexer(p->lex);

    p->lex = obs_alloc(&p->pool_, sizeof(*p->lex));
    init_lexer(&p->pool_, p->lex, pathname);

    parse(p->lex, &p->root);
  }

  return p;
}


void
properties_close(PROPERTIES *props)
{
  struct property *p, *tmp;

  HASH_ITER(hh, props->root, p, tmp) {
    HASH_DEL(props->root, p);
  }

  free_lexer(props->lex);
  obs_free(&props->pool_, NULL);
  free(props);
}


void
properties_put(PROPERTIES *props, const char *key, const char *value)
{
  struct property *p;

  HASH_FIND_STR(props->root, key, p);
  if (p)
    HASH_DEL(props->root, p);

  p = obs_alloc(&props->pool_, sizeof(*p));
  p->key = obs_copy0(&props->pool_, key, strlen(key));
  p->value = obs_copy0(&props->pool_, value, strlen(value));

  HASH_ADD_KEYPTR(hh, props->root, p->key, strlen(p->key), p);
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
                void *data)
{
  struct property *p, *tmp;
  int count = 0;

  HASH_ITER(hh, props->root, p, tmp) {
    if (iter(p->key, p->value, data) == -1)
      break;
    count++;
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

  properties_enum(props, myiter, NULL);

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
