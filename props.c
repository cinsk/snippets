#define _GNU_SOURCE     1
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>

#define HAVE_UNISTRING

#ifdef HAVE_UNISTRING
#include <unistr.h>
#include <unictype.h>
#else
typedef uint32_t ucs4_t;
#endif  /* HAVE_UNISTRING */

#include "uthash.h"
#include "xerror.h"


/*
 * TODO:
 *
 * 1. A comment that has an '#' or '!' as its first non-white space
 *    character.  Currently, only '#' or '!' as the first character of
 *    the line is recognized.
 *
 * 2. escaping ':' or '=' for (key, value) delimiter is not supported.
 *
 * 3. triming around the UTF-8 line is not supported yet (even with
 *    libunistring).
 */
#define PROP_WHITESPACES        " \t\n\v\f\r"

#ifndef FALSE
#define FALSE   0
#define TRUE    (!FALSE)
#endif

struct property_ {
  char *key;
  char *value;

  UT_hash_handle hh;
};

typedef struct property_ PROPERTIES;

#define SSTREAM_CHUNK_SIZE      4096

struct c_strstream {
  char *body;
  size_t capacity;

  char *pos;
};


int
init_sstream(struct c_strstream *stream)
{
  stream->body = malloc(SSTREAM_CHUNK_SIZE);
  if (!stream->body)
    return -1;
  stream->capacity = SSTREAM_CHUNK_SIZE;
  stream->pos = stream->body;

  return 0;
}


int
reserve_sstream(struct c_strstream *stream, size_t size)
{
  char *p;
  size_t new_cap;
  int needed;

  if (stream->capacity < (stream->pos - stream->body) + size) {
    needed = (stream->pos - stream->body) + size - stream->capacity;

    new_cap = (needed + SSTREAM_CHUNK_SIZE - 1);
    new_cap = new_cap / SSTREAM_CHUNK_SIZE * SSTREAM_CHUNK_SIZE;
    new_cap += stream->capacity;

    p = realloc(stream->body, new_cap);
    if (!p)
      return -1;
    else {
      if (!stream->body)
        stream->pos = stream->body;
      else
        stream->pos = p + (stream->pos - stream->body);

      stream->capacity = new_cap;
      stream->body = p;
    }
  }
  return 0;
}


__inline__ int
addc_sstream(struct c_strstream *stream, char ch)
{
  if (reserve_sstream(stream, 2) == -1)
    return -1;

  *stream->pos = ch;
  stream->pos++;
  *stream->pos = '\0';

  return 0;
}


int
add_sstream(struct c_strstream *stream, const char *s)
{
  size_t sz = strlen(s);

  if (reserve_sstream(stream, sz + 1) == -1)
    return -1;

  memcpy(stream->pos, s, sz);

  stream->pos += sz;
  *stream->pos = '\0';
  return 0;
}


int
addf_sstream(struct c_strstream *stream, const char *fmt, ...)
{
  va_list ap;
  int sz;
  char *buf = NULL;

  va_start(ap, fmt);
  sz = vasprintf(&buf, fmt, ap);
  va_end(ap);

  if (reserve_sstream(stream, sz + 1) == -1)
    return -1;

  memcpy(stream->pos, buf, sz);

  stream->pos += sz;
  *stream->pos = '\0';

  free(buf);
  return 0;
}


int
add_sstream_u8escaped(struct c_strstream *stream, const char *s)
{
  const char *p;
  char ahead[5] = { 0, };
#ifdef HAVE_UNISTRING
  ucs4_t unic;
  uint8_t unic_u8[MB_LEN_MAX];
  int convlen;
  size_t slen = strlen(s);
  char *endptr;
#endif  /* HAVE_UNISTRING */
  int retval = 0;

  for (p = s; *p != '\0'; p++) {
    if (*p != '\\')
      addc_sstream(stream, *p);
    else {                      /* process the escape sequence */
      ahead[0] = *(p + 1);

      switch (ahead[0]) {
      case 'a':
        addc_sstream(stream, '\a'); p++;
        break;
      case 'b':
        addc_sstream(stream, '\b'); p++;
        break;
      case 't':
        addc_sstream(stream, '\t'); p++;
        break;
      case 'n':
        addc_sstream(stream, '\n'); p++;
        break;
      case 'v':
        addc_sstream(stream, '\v'); p++;
        break;
      case 'f':
        addc_sstream(stream, '\f'); p++;
        break;
      case 'r':
        addc_sstream(stream, '\r'); p++;
        break;
      case '\\':
        addc_sstream(stream, '\\'); p++;
        break;
#if 0
      case '=':
        addc_sstream(stream, '='); p++;
        break;
      case ':':
        addc_sstream(stream, ':'); p++;
        break;
#endif        /* 0 */
      case 0:                   /* escaping line terminator */
        retval = 1;
        break;

#ifdef HAVE_UNISTRING
      case 'u':
        if (slen - (p + 2 - s) > 4) {
          memcpy(ahead, p + 2, 4);
          unic = strtoul(ahead, &endptr, 16);
          if (*endptr != '\0')
            p += 2 + endptr - ahead;
          else
            p += 1 + 4;
        }
        else {
          unic = strtoul(p + 2, &endptr, 16);
          p = endptr - 1;
        }

        if (unic != 0) {
          convlen = u8_uctomb(unic_u8, unic, MB_LEN_MAX);
          if (convlen < 0) {
            fprintf(stderr, "converting %04x to UTF-8 failed\n", unic);
          }
          else {
            unic_u8[convlen] = '\0';
            add_sstream(stream, (char *)unic_u8);
          }
        }
        break;
#endif  /* HAVE_UNISTRING */
      default:
        addf_sstream(stream, "\\%c", ahead[0]);
        p++;
        break;
      }
    }
  }
  return retval;
}


int
add_sstream_u8(struct c_strstream *stream, uint8_t *s, size_t n)
{
  size_t sz = strlen((char *)s);

  if (reserve_sstream(stream, sz + 1) == -1)
    return -1;

  memcpy(stream->pos, s, sz);

  stream->pos += sz;
  *stream->pos = '\0';
  return 0;
}


char *
str_sstream(struct c_strstream *stream)
{
  return stream->body;
}


void
del_sstream(struct c_strstream *stream)
{
  free(stream->body);
  stream->body = 0;
  stream->pos = 0;
  stream->capacity = 0;
}


void
clear_sstream(struct c_strstream *stream)
{
  stream->pos = stream->body;
  *stream->pos = '\0';
}


int
isempty_sstream(struct c_strstream *stream)
{
  return (stream->pos == stream->body);
}


int
props_put(PROPERTIES **props, const char *key, const char *value)
{
  struct property_ *p;

  p = malloc(sizeof(*p));
  if (!p)
    return -1;
  p->key = strdup(key);
  if (!p->key) {
    free(p);
    return -1;
  }
  p->value = strdup(value);
  if (!p->value) {
    free(p->key);
    free(p);
    return -1;
  }
  HASH_ADD_KEYPTR(hh, *props, p->key, strlen(p->key), p);

  return 0;
}


static int
props_parse_line(char *src, char **key, char **value)
{
  char *pivot = src;
  size_t begin;
  char *q;
  size_t srclen = strlen(src);
  //int pivot_found = FALSE;

  begin = strspn(src, PROP_WHITESPACES);
  pivot += begin;

  *key = pivot;

#if 0
  for (p = pivot; *p != '\0'; p++) {
    // See also http://docs.oracle.com/javase/7/docs/api/java/util/Properties.html#load(java.io.Reader)

    if (*p == '\\') {
      ;
    }
  }
#endif  /* 0 */

  /* TODO: this is bad.  I need to process escaped '=' or ':' */


  pivot = strchr(pivot, L'=');

  if (!pivot) {
    *key = '\0';
    *value = '\0';
    return -1;
  }

  *pivot = '\0';

  for (q = pivot - 1; q >= *key && isspace(*q); q--)
    ;
  *(q + 1) = '\0';              /* KEY is nul-terminated */

  *value = pivot + 1;
  begin = strspn(*value, PROP_WHITESPACES);
  *value += begin;

  for (q = src + srclen - 1; q >= *value && isspace(*q); q--)
    ;
  *(q + 1) = '\0';              /* VALUE is nul-terminated */

  return 0;
}


char *
strtrim_left_inp(char *s)
{
  size_t n = strspn(s, " \t\n\v\f\r");
  return s + n;
}


char *
strtrim_right_inp(char *s)
{
  char *p;
  size_t n = strlen(s);

  if (n == 0)
    return s;

  for (p = s + n - 1; p >= s; p--) {
    switch (*p) {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '\v':
    case '\f':
      *p = '\0';
      break;
    default:
      return s;
    }
  }
  return s;
}


char *
strtrim_inp(char *s)
{
  return strtrim_right_inp(strtrim_left_inp(s));
}


#if 0
uint8_t *
u8_strtrim_left(uint8_t *s, size_t n)
{
  ucs4_t uc;
  int nread;
  uint8_t *p, *candidate;

  p = s;
  candidate = 0;

  while (n > 0) {
    nread = u8_mbtouc(&uc, p, n);

    if (nread == -1)
      return NULL;

    if (!uc_is_general_category(uc, UC_SPACE_SEPARATOR) &&
        !uc_is_general_category(uc, UC_CONTROL))
      return p;

    p += nread;
    n -= nread;
  }

  return s;
}


uint8_t *
u8_strtrim_right(uint8_t *s, size_t n)
{
  ucs4_t uc;
  int nread;
  uint8_t *p, *candidate;
  int met;

  p = s;
  candidate = 0;

  while (n > 0) {
    nread = u8_mbtouc(&uc, p, n);

    if (nread == -1)
      return NULL;

    if (uc_is_general_category(uc, UC_SPACE_SEPARATOR) ||
        uc_is_general_category(uc, UC_CONTROL)) {
      if (!met) {
        met = TRUE;
        candidate = p;
      }
    }
    else
      met = FALSE;

    p += nread;
    n -= nread;
  }

  if (met)
    *candidate = '\0';

  return s;
}


uint8_t *
u8_strtrim(uint8_t *s, size_t n)
{
  uint8_t *p;

  p = u8_strtrim_left(s, n);
  if (!p)
    return NULL;

  n -= (p - s);
  return u8_strtrim_right(p, n);
}
#endif  /* 0 */


PROPERTIES *
props_load(const char *pathname, FILE *errlog, PROPERTIES *props)
{
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;

  char *key;
  char *value;

  struct c_strstream ss;
  int lineno = 0;

  FILE *fp;

  fp = fopen(pathname, "r");
  if (!fp)
    return NULL;

  init_sstream(&ss);

  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    lineno++;

    if (line[linelen - 1] == '\n') {
      line[linelen - 1] = '\0';
      linelen--;
    }

    if (line[0] == '\0' || line[0] == '!' || line[0] == '#')
      continue;

    /* TODO: '!' and '#' can appear at the first non-whitespace character */

    if (add_sstream_u8escaped(&ss, line) > 0) {
      /* expand to the next line */
      continue;
    }

    props_parse_line(str_sstream(&ss), &key, &value);
    if (errlog && (!key || !value)) {
      fprintf(errlog, "%s:%d: parse error\n", pathname, lineno);
    }
    else
      props_put(&props, key, value);

    //fprintf(stderr, "[%s] = [%s]\n", key, value);

    clear_sstream(&ss);
  }

  del_sstream(&ss);
  free(line);
  fclose(fp);

  return props;
}


void
props_free(PROPERTIES *props)
{
  PROPERTIES *s, *tmp;
  HASH_ITER(hh, props, s, tmp) {
    HASH_DEL(props, s);
    free(s->key);
    free(s->value);
    free(s);
  }
}

const char *
props_get(PROPERTIES *props, const char *key)
{
  PROPERTIES *p;

  HASH_FIND_STR(props, key, p);

  if (p)
    return p->value;
  else
    return NULL;
}


int
main_(int argc, char *argv[])
{
  struct c_strstream ss;

  init_sstream(&ss);

  add_sstream_u8escaped(&ss, argv[1]);

  printf("|%s|\n", str_sstream(&ss));
  del_sstream(&ss);

  return 0;
}


int
main(int argc, char *argv[])
{
  PROPERTIES *props = NULL;

  props = props_load(argv[1], stderr, NULL);

  {
    PROPERTIES *s, *tmp;
    HASH_ITER(hh, props, s, tmp) {
      printf("[%s] = |%s|\n", s->key, s->value);
      //HASH_DEL(props, s);
    }
  }

  props_free(props);

  return 0;
}


int
main5(int argc, char *argv[])
{
  char s[] = " \n ​ 안녕 \n\t";
  //printf("|%s|\n", u8_strtrim(s, strlen(s)));
  printf("|%s|\n", strtrim_inp(s));

  struct c_strstream ss;

  init_sstream(&ss);

  add_sstream(&ss, "asdf");
  add_sstream(&ss, "qwer");
  add_sstream(&ss, "zxcv");

  printf("|%s|\n", str_sstream(&ss));

  return 0;
}


#if 0
int
main1(int argc, char *argv[])
{
  uint8_t *src = (uint8_t *)argv[1];
  uint8_t *p = src;
  ucs4_t uc;
  size_t len = strlen(src);
  uc_general_category_t category;

  while (*p != '\0') {
    int nread = u8_mbtouc(&uc, p, len);

    printf("nread: %d\n", nread);
    if (nread != -1) {
      p += nread;


      category = uc_general_category(uc);
      printf("category: %s\n", uc_general_category_name(category));
    }
  }

  return 0;
}

int
main2(int argc, char *argv[])
{
  int ret;
  uint8_t *key, *value;

  ret = props_parse_line(argv[1], &key, &value);

  printf("key = |%s|\n", key);
  printf("value = |%s|\n", value);
  printf("ret = %d\n", ret);

  return 0;
}
#endif  /* 0 */
