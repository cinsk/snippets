#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "buffer.h"


BUFFER *
buf_open(void *data, size_t size, int flags)
{
  BUFFER *p;

  p = malloc(sizeof(*p));
  if (!p)
    return NULL;

  p->flags = flags;

  if (BUF_GROW(p)) {
    p->unit = sysconf(_SC_PAGESIZE);
    if (p->unit == -1)
      p->unit = BUFSIZ;
  }
  else
    p->unit = 0;

  if (data) {
    p->data = (char *)data;
    p->end = p->data + size;
  }
  else {
    assert(BUF_GROW(p));

    p->data = malloc(p->unit);
    if (!p->data) {
      free(p);
      return NULL;
    }
    p->end = p->data + p->unit;
  }

  p->pos = p->data;
  p->lim = p->end;
  p->mark = NULL;

  return p;
}


BUFFER *
buf_new(void)
{
  return buf_open(NULL, 0, BF_GROW | BF_FREE);
}


void
buf_close(BUFFER *bp)
{
  if (BUF_FREE(bp))
    free(bp->data);
  free(bp);
}


void
buf_flip(BUFFER *bp)
{
  bp->lim = bp->pos;
  bp->pos = bp->data;
  bp->mark = NULL;

  //*bp->lim = '\0';
}


void
buf_clear(BUFFER *bp)
{
  bp->pos = bp->data;
  bp->lim = bp->end;
  bp->mark = NULL;
}


int
buf_seek(BUFFER *bp, long offset, int whence)
{
  char *newpos;

  switch (whence) {
  case SEEK_SET:
    newpos = bp->data + offset;
    break;
  case SEEK_CUR:
    newpos = bp->pos + offset;
    break;
  case SEEK_END:
    newpos = bp->lim + offset;
    break;
  default:
    return -1;
  }
  if (newpos >= bp->data && newpos < bp->lim) {
    bp->pos = newpos;
    return 0;
  }
  return -1;
}


long
buf_tell(BUFFER *bp)
{
  return bp->pos - bp->data;
}


int
buf_printf(BUFFER *bp, const char *format, ...)
{
  va_list ap;
  int written;

  va_start(ap, format);
  written = buf_vprintf(bp, format, ap);
  va_end(ap);

  return written;
}


int
buf_vprintf(BUFFER *bp, const char *format, va_list ap)
{
  int needed;
  int avail, written;

  needed = vsnprintf(bp->pos, 0, format, ap);

  avail = bp->end - bp->pos;

  buf_grow(bp, needed + 1);     /* 1 for null-terminated string */
  avail = bp->end - bp->pos;

  written = vsnprintf(bp->pos, avail, format, ap);
  if (written >= avail) {       /* truncated */
    bp->pos += avail;
    return -avail;
  }
  else if (written >= 0)
    bp->pos += written;
  else {
    /* can't happend;  vsnprintf() cannot return a negative value */
    abort();
  }

  /* TODO: Do I need to make null-terminated string? */
  if (written < avail)
    *bp->pos = '\0';

  return written;
}


#if 0
int
buf_scanf(BUFFER *bp, const char *format, ...)
{
  va_list ap;
  int read;

  va_start(ap, format);
  //read = buf_vscanf(bp, format, ap);
  va_end(ap);

  return read;
}


int
buf_vscanf(BUFFER *bp, const char *format, ...)
{
  int read;

  //read = vsscanf(bp->pos, format, ap);
}
#endif  /* 0 */


size_t
buf_write(const void *ptr, size_t size, size_t nmemb, BUFFER *bp)
{
  size_t needed = size * nmemb;
  size_t avail = bp->end - bp->pos;

  if (avail < needed) {
    buf_grow(bp, needed);
  }
  avail = bp->end - bp->pos;

  if (needed > avail)
    needed = avail / size * size;

  memcpy(bp->pos, ptr, needed);
  bp->pos += needed;

  /* TODO: do I need to adjust bp->lim? */

  return needed / size;

}


size_t
buf_read(void *ptr, size_t size, size_t nmemb, BUFFER *bp)
{
  size_t needed = size * nmemb;
  size_t avail;

  if (bp->pos < bp->lim) {
    avail = bp->lim - bp->pos;

    if (needed > avail)
      needed = avail / size * size;

    memcpy(ptr, bp->pos, needed);
    bp->pos += needed;
    return needed / size;
  }
  else
    return 0;
}


int
buf_flush(BUFFER *bp)
{
  buf_grow(bp, 1);

  if (bp->pos < bp->end) {
    if (*bp->pos != '\0')
      *bp->pos = '\0';
    return 0;
  }

  return -1;
}


int
buf_getc(BUFFER *bp)
{
  if (bp->pos < bp->lim)
    return *bp->pos++;
  else
    return EOF;
}


char *
buf_gets(char *s, int size, BUFFER *bp)
{
  size_t lim;
  char *p;

  /* TODO: not tested! */
  assert(bp->pos <= bp->lim);

  if (bp->pos >= bp->lim)
    return NULL;

  lim = bp->lim - bp->pos;
  if (lim > size - 1)
    lim = size - 1;

  p = memchr(bp->pos, '\n', lim);

  if (p) {
    memcpy(s, bp->pos, p - bp->pos + 1);
    s[p - bp->pos + 1 + 1] = '\0';
    bp->pos = p + 1;
  }
  else {                        /* no new line */
    memcpy(s, bp->pos, lim);
    s[lim] = '\0';
    bp->pos += lim;
  }
  return s;
}


void
buf_mark(BUFFER *bp)
{
  bp->mark = bp->pos;
}


int
buf_reset(BUFFER *bp)
{
  if (bp->mark == NULL)
    return -1;

  bp->pos = bp->mark;
  return 0;
}


char *
buf_region_string(BUFFER *bp)
{
  char *s;
  size_t len;

  if (bp->mark == NULL)
    return NULL;

  assert(bp->lim >= bp->data);
  assert(bp->lim <= bp->end);
  assert(bp->lim >= bp->mark);

  len = bp->lim - bp->mark;
  s = malloc(len + 1);
  if (!s)
    return NULL;

  memcpy(s, bp->mark, len);
  s[len] = '\0';

  return s;
}


void *
buf_region(BUFFER *bp, size_t *size)
{
  if (bp->mark == NULL)
    return NULL;

  assert(bp->lim >= bp->data);
  assert(bp->lim <= bp->end);
  assert(bp->lim >= bp->mark);

  if (size)
    *size = bp->lim - bp->mark;

  return bp->mark;
}


char *
buf_substring(BUFFER *bp, off_t start, off_t end)
{
  size_t len;
  char *s;

  assert(start >= 0);
  assert(end >= 0);
  assert(start <= end);

  if (bp->pos + start >= bp->lim)
    return NULL;

  len = end - start;
  if (bp->pos + len >= bp->lim)
    len = bp->lim - bp->pos;

  s = malloc(len + 1);
  memcpy(s, bp->pos, len);
  s[len] = '\0';

  return s;
}


/*
   buf_read_re(int advance, regex_t ....);
   buf_read_pcre(int advance, regex_t ....);

   buf_read_cb(int advance, int (*cb)(char *ptr, size_t size, void *data),
               void *data);

   mark() -> region_as_string();
   mark() -> void *region(size_t *size);
   mark() -> revert() // go back to mark to re-read

 */


#ifdef _TEST_BUFFER

int
main(int argc, char *argv[])
{
  BUFFER *bp;
  int i, ch, ret;
  char long_string[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  char *chunk = malloc(10);


  bp = buf_open(chunk, 10, 0);

  for (i = 0; i < 10; i++) {
    ret = buf_printf(bp, "%08x\n", i);
    printf("writing %08x, got return value, %d\n", i, ret);
  }

  buf_clear(bp);

  for (i = 0; i < 11; i++) {
    ret = buf_putc('a' + i, bp);
    printf("writing %c, got return value, %d\n", 'a' + i, ret);
  }

  buf_clear(bp);

  ret = buf_puts(long_string, bp);
  printf("writing long string, got return value, %d\n", ret);
  ret = buf_puts(long_string, bp);
  printf("writing long string, got return value, %d\n", ret);

  buf_close(bp);

  bp = buf_new();

  buf_printf(bp, "hello there %s\n", "cinsk");
  buf_flush(bp);

  buf_flip(bp);

  while ((ch = buf_getc(bp)) != EOF)
    putchar(ch);

  buf_seek(bp, 0, SEEK_SET);

  while ((ch = buf_getc(bp)) != EOF)
    putchar(ch);


  {
    //char *s;
    //double *f;
    //SSCANF(bp->pos, "%*lf %*10s");
    int i, r;

    buf_clear(bp);
    buf_printf(bp, "43.32");
    buf_flip(bp);
    r = buf_scanf(bp, "%d", &i);

    printf("i = %d, r = %d\n", i, r);
  }

  buf_clear(bp);
  buf_puts("hello\n", bp);
  buf_puts("there\n", bp);
  buf_puts("world\n", bp);
  buf_flip(bp);
  {
    char line[4];
    while (buf_gets(line, 4, bp) != 0)
      printf("%s", line);
  }

  buf_close(bp);

  return 0;
}
#endif  /* _TEST_BUFFER */
