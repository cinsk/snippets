#ifndef BUFFER_H__
#define BUFFER_H__

#include <stddef.h>
#include <stdarg.h>

/*
 * This module provides BUFFER type and related functions and macros.
 *
 * BUFFER is somewhat similar to C11's string stream, and also similar to
 * Java's Buffer class.
 *
 * BUFFER will maintain its internal memory chunk, which can dynamically
 * grow itself if needed.
 *
 * Normally, Some operation does not guarantees that the contents of
 * the buffer is null-terminated string.  Calling buf_flush() after
 * writing operation guarantees that the internal buffer content is
 * null-terminated.
 */

/* write/read buffer, writing is unlimited */
struct buffer {
  char *data;                   /* points to the beginning of the mem chunk */

  char *pos;                    /* points to the current position of the
                                 * read or write */
  char *end;                    /* points to the 1 past of the mem chunk */

  char *lim;                    /* points to the end of the writing */

  char *mark;

  int flags;
  //char grow;                     /* nonzero if DATA can grow (realloc) */
  //char free;                     /* nonzero if DATA should be freed */

  size_t unit;
};


typedef struct buffer BUFFER;

#define BF_GROW 0x01
#define BF_FREE 0x02

#define BUF_GROW(buf)   ((buf)->flags & BF_GROW)
#define BUF_FREE(buf)   ((buf)->flags & BF_FREE)

/*
 * Create new BUFFER.
 *
 * The internal memory chunk will be automatically created, and it
 * will grow automatically.  Finally, the memory chunk will be release
 * when you call buf_close().
 */
BUFFER *buf_new(void);

/*
 * Create new BUFFER from existing memory chunk.
 *
 * buf_open() will create BUFFER object without creating its own
 * memory chunk.  instead, it just wrap the given memory block pointed
 * by DATA (with size, SIZE).
 *
 * FLAGS can be zero or more of BF_* flags.  If BF_GROW is specified,
 * DATA must be the pointer returned from either malloc(3) or
 * calloc(3), and subsequent calls to buf_* may grow/shrink the memory
 * chunk.
 *
 * If BF_FREE is specified, DATA will be release by calling free(3)
 * when buf_close() is called.
 */
BUFFER *buf_open(void *data, size_t size, int flags);

void buf_close(BUFFER *bp);

int buf_vprintf(BUFFER *bp, const char *format, va_list ap);
int buf_printf(BUFFER *bp, const char *format, ...)
  __attribute__ ((format (printf, 2, 3)));

void buf_flip(BUFFER *bp);
int buf_seek(BUFFER *bp, long offset, int whence);
long buf_tell(BUFFER *bp);
int buf_flush(BUFFER *bp);
void buf_mark(BUFFER *bp);
int buf_reset(BUFFER *bp);

/*
 * Return a pointer to the beginning of the region.
 *
 * It returns NULL if there is no mark.
 * If SIZE is non-zero, it will be set to the size of the region.
 *
 * This function does not allocate memory for the region.  Thus, writing
 * to the BP may invalidate the region.
 */
void *buf_region(BUFFER *bp, size_t *size);

/*
 * Return a region as string.
 *
 * The caller must call free(3) to deallocate the string.
 * It returns NULL if there is no mark.
 */
char *buf_region_string(BUFFER *bp);


size_t buf_read(void *ptr, size_t size, size_t nmemb, BUFFER *bp);
size_t buf_write(const void *ptr, size_t size, size_t nmemb, BUFFER *bp);

/*
 * Grow or shrink the mem chunk.
 *
 * If SIZE is non-zero, buf_grow() will grow the mem chunk so that the
 * BP can hold SIZE byte(s).  All members of BP will be adjusted
 * accordingly.
 *
 * If SIZE is zero, buf_grow() will shrink the mem chunk so that only
 * bytes between BP->data and BP->pos are preserved.  BP->end and
 * BP->lim will be adjusted to reflect the shrinked mem chunk.
 */
static __inline__ int
buf_grow(BUFFER *bp, size_t size)
{
  size_t newsize;
  char *p;

  if (!BUF_GROW(bp))
    return -1;

  if (size) {
    if (bp->end - bp->pos > size)
      return 0;

    // newsize = (((bp->end - bp->data) + size - (bp->end - bp->pos)) / bp->unit + 1) * bp->unit

    newsize = ((size + bp->pos - bp->data) / bp->unit + 1) * bp->unit;
  }
  else {
    newsize = ((bp->pos - bp->data) / bp->unit + 1) * bp->unit;
  }

  p = realloc(bp->data, newsize);
  if (!p)
    return -1;

  bp->pos = bp->pos - bp->data + p;
  bp->mark = bp->mark - bp->data + p;

  if (size) {
    bp->lim = bp->lim - bp->data + p;
    bp->data = p;
    bp->end = p + newsize;
  }
  else {
    bp->data = p;
    bp->lim = bp->end = p + newsize;
  }

  return 0;
}


static __inline__ int
buf_putc(int c, BUFFER *bp)
{
  buf_grow(bp, 1);

  if (bp->pos < bp->end) {
    *bp->pos++ = (unsigned char)c;
    return (unsigned char)c;
  }
  else
    return EOF;
}


static __inline__ int
buf_puts(const char *s, BUFFER *bp)
{
  size_t len = strlen(s);

  buf_grow(bp, len + 1);

  if (bp->pos + len < bp->end) {
    strcpy(bp->pos, s);
    bp->pos += len;
    return len;
  }
  else if (bp->pos < bp->end) {
    memcpy(bp->pos, s, bp->end - bp->pos);
    bp->pos = bp->end;
    return EOF;
  }
  else
    return EOF;
}



#define buf_scanf(b, f, ...)        ({ int __readch__, __return__;      \
      char *p = alloca(strlen(f) + 3);                                  \
      sprintf(p, "%s%%n", (f));                                         \
      __return__ = sscanf((b)->pos, p, ##__VA_ARGS__, &__readch__);     \
      if (__return__ != EOF)                                            \
        (b)->pos += __readch__;                                         \
      __return__; })


/*
 * Return a substring starting from START to END.
 *
 * The actual range is between START + current buffer position to
 * END + current buffer position.
 *
 * If START + current buffer position is out of the limit (i.e. equal
 * to or greater than BP->lim), it returns NULL.  If END + current
 * buffer position is out of the limit, the actual range will be
 * between START + current buffer position to BP->lim.
 *
 * The caller must call free(3) to release the returned string.
 */
char *buf_substring(BUFFER *bp, off_t start, off_t end);

/*
 * Returns the pointer to the current position of the buffer, BP.
 * This macro does not guarantee that the content of the buffer is
 * null-terminated.  Either you should call buf_flush() prior to call
 * this macro, or put '\0' by yourself.
 */
#define buf_position(bp)        ((bp)->pos)


#endif  /* BUFFER_H__ */
