/* $Id$ */
/*
 * Light file stream
 * Copyright (C) 2004  Seong-Kook Shin <cinsky@gmail.com>
 */
#include <stream.h>
#include <xassert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define STREAM_BUFSIZ 3

#ifndef STREAM_BUFSIZ
# ifdef BUFSIZ
#  define STREAM_BUFSIZ BUFSIZ
# else
#  define STREAM_BUFSIZ 4096
# endif
#endif /* STREAM_BUFSIZ */

int stream_errno;

struct stream_ {
  struct stream_ops op;
  int type;
  unsigned flags;

  off_t vpos;                   /* virtual file position,
                                 * vpos is the file position of CUR */
  off_t ppos;                   /* physical file position */

  unsigned b_mode;              /* buffer mode */
  char *end;                    /* 1 plus end of the valid data in BUF */
  char *buf;                    /* points the internal buffer */
  size_t size;                  /* size of the buffer */
  char *cur;                    /* current pos of BUF */

  int ungetc;

  int fd;
  void *data;

  unsigned eof:   1;            /* nonzero if EOF is reached */
  unsigned dirty: 1;            /* nonzero if BUF contains unwritten data */
};

static char stream_nbuf[1];

/*
 *      READ    WRITE   CREATE  TRUNC   POS     MISC
 * "r"  yes     no      no      -       begin   -
 * "r+" yes     yes     no      -       begin   -
 * "w"  no      yes     yes     yes     begin   -
 * "w+" yes     yes     yes     yes     begin   -
 * "a"  no      yes     yes     no      end
 * "a+" yes     yes     yes     no      end     writing always at the end.
*/
enum {
  ST_READ,
  ST_READ_P,
  ST_WRITE,
  ST_WRITE_P,
  ST_APPEND,
  ST_APPEND_P,
  ST_NUMBER,
};

static int stream_flags[ST_NUMBER] = {
  O_RDONLY,
  O_RDWR | O_CREAT,
  O_WRONLY | O_CREAT | O_TRUNC,
  O_RDWR | O_CREAT | O_TRUNC,
  O_WRONLY | O_CREAT,
  O_RDWR | O_CREAT,
};

static int get_type_determined(const char *mode);
static int get_flags_from_type(int type);
static int flush_buf(stream_t *s);
static int get_buf_prepared(stream_t *s);


stream_t *
s_open(const struct stream_ops *ops, const char *pathname, const char *mode,
       void *data)
{
  stream_t *s;

  s = malloc(sizeof(*s));
  if (!s)
    return 0;

  memcpy(&s->op, ops, sizeof(*ops));
  s->type = get_type_determined(mode);
  s->flags = get_flags_from_type(s->type);
  s->data = data;
  s->fd = s->op.open(pathname, s->flags, s->data);
  if (s->fd == -1) {
    stream_errno = errno;
    goto err;
  }
  s->ungetc = -1;

  s->dirty = 0;
  s_setvbuf(s, malloc(STREAM_BUFSIZ), STREAM_IOFBF, STREAM_BUFSIZ);

  /* if (!(s->flags & O_TRUNC) && get_buf_prepared(s) < 0) { */
  if (get_buf_prepared(s) < 0) {
    s->op.close(s->fd, s->data);
    goto err;
  }
  return s;

 err:
  free(s);
  return 0;

}


int
s_close(stream_t *s)
{
  s_setvbuf(s, 0, STREAM_IONBF, 0);

  if (s->op.close(s->fd, s->data) == -1) {
    stream_errno = errno;
    return -1;
  }
  free(s);
  return 0;
}


/*
 * Differences from setvbuf(3)
 * - buffer changing takes effect immediately.
 * - set stream_errno instead of errno(3).
 * - Whenver s_setvbuf() replaces the existing buffer, it calls free().
 *   This means new buffer should be allocated by calling malloc().
 */
int
s_setvbuf(stream_t *s, char *buf, int mode, size_t size)
{
  xassert(mode == STREAM_IONBF || mode == STREAM_IOLBF || mode == STREAM_IOFBF,
          "MODE must be one of STREAM_IONBF, STREAM_IOLBF, or STREAM_IOFBF");

  if (mode == STREAM_IONBF) {
    if (s->dirty && flush_buf(s) < 0)
      return -1;
    if (s->buf != stream_nbuf)
      free(s->buf);

    buf = stream_nbuf;
    s->size = 1;
    s->end = buf + 1;
    s->cur = 0;
    s->b_mode = mode;
    return 0;
  }

  if (buf) {
    if (s->dirty)
      flush_buf(s);

    if (s->buf != stream_nbuf)
      free(s->buf);

    s->buf = buf;
    s->size = size;
    s->end = buf + 1;
    s->cur = s->end;
  }
  s->b_mode = mode;
  return 0;
}


off_t
s_seek(stream_t *s, off_t offset, int whence)
{
  /* TODO: not implemented yet */
  return -1;
}


int
s_getc(stream_t *s)
{
  xassert(s->type != ST_WRITE && s->type != ST_APPEND,
          "attempt to read from write-only stream");

  if (s->cur >= s->end) {
    if (get_buf_prepared(s) < 0 || s->eof)
      return EOF;
  }
  s->vpos++;
  return *s->cur++;
}


int
s_putc(stream_t *s, int c)
{
  xassert(s->type != ST_READ, "attempt to write on read-only stream");

  if (s->cur == s->end) {
    if (s->end == s->buf + s->size) {
      if (get_buf_prepared(s) < 0)
        return EOF;
    }
    s->end++;
  }

  s->dirty = 1;
  s->vpos++;
  *s->cur++ = c;

  return (unsigned)c;
}


/*
 * Get an internal type value for MODE string.
 */
static int
get_type_determined(const char *mode)
{
  switch (mode[0]) {
  case 'r':
    return (mode[1]) ? ST_READ_P : ST_READ;
  case 'w':
    return (mode[1]) ? ST_WRITE_P : ST_WRITE;
  case 'a':
    return (mode[1]) ? ST_APPEND_P : ST_APPEND;
  default:
    return (unsigned)-1;
  }
}


/*
 * Get a combination of system flags from the internal type.
 */
static int
get_flags_from_type(int type)
{
  xassert(type >= 0 && type < ST_NUMBER, "invalid argument, type(%d)", type);

  return stream_flags[type];
}


/*
 * Flush the internal buffer and clean DIRTY flag.
 *   (char range: from stream_t::buf to stream_t::end - 1
 */
static int
flush_buf(stream_t *s)
{
  off_t pos;
  int written;

  xassert(s->cur != (char *)-1, "s->buf is not valid");

  pos = s->vpos - (s->cur - s->buf);
  if (s->ppos != pos)
    if (s->op.lseek(s->fd, pos, SEEK_SET, s->data) == (off_t)-1) {
      stream_errno = errno;
      return -1;
    }
  written = s->op.write(s->fd, s->buf, s->cur - s->buf, s->data);
  if (written == -1) {
    stream_errno = errno;
    return -1;
  }
  if (written != s->cur - s->buf) {
    /* TODO: support for async I/O? */
    xassert(0, "TODO: support for async I/O?");
  }
  s->ppos = s->vpos;
  s->dirty = 0;
  s->cur = s->end = s->buf;
  return 0;
}


/*
 * Load new contents from the file with the offset, stream_t::vpos if needed.
 * and set CUR, END, PPOS appropriately.
 */
static int
get_buf_prepared(stream_t *s)
{
  /* Should be called when:
   *  1. s->cur == s->end
   */
  int chread = 0;

  if (s->dirty && flush_buf(s) < 0)
    return -1;

  if (s->ppos != s->vpos) {
    if (s->op.lseek(s->fd, s->vpos, SEEK_SET, s->data) == (off_t)-1) {
      stream_errno = errno;
      return -1;
    }
    s->ppos = s->vpos;
  }
  if (!s->eof && s->type != ST_WRITE && s->type != ST_APPEND) {
    chread = s->op.read(s->fd, s->buf, s->size, s->data);
    if (chread < 0)
      return -1;
    if (!chread)
      s->eof = 1;
  }
  s->cur = s->buf;
  s->end = s->buf + chread;
  s->ppos += chread;
  return 0;
}


#ifdef TEST_STREAM

static int
posix_open(const char *pathname, int flags, void *data)
{
  return open(pathname, flags, 0666);
}

static int
posix_close(int fd, void *data)
{
  return close(fd);
}

static ssize_t
posix_read(int fd, void *buf, size_t count, void *data)
{
  return read(fd, buf, count);
}

static ssize_t
posix_write(int fd, const void *buf, size_t count, void *data)
{
  return write(fd, buf, count);
}

static off_t
posix_lseek(int fd, off_t offset, int whence, void *data)
{
  return lseek(fd, offset, whence);
}

struct stream_ops posix_ops = {
  posix_open,
  0,
  posix_close,
  posix_read,
  posix_write,
  posix_lseek
};

int
main(int argc, char *argv[])
{
  stream_t *s_r, *s_w;
  int ch;


  s_r = s_open(&posix_ops, argv[1], "r", 0);
  if (!s_r) {
    fprintf(stderr, "error: cannot open a stream for %s\n", argv[1]);
    return 1;
  }

  s_w = s_open(&posix_ops, argv[2], "w", 0);
  if (!s_w) {
    fprintf(stderr, "error: cannot open a stream for %s\n", argv[2]);
    return 1;
  }

  while ((ch = s_getc(s_r)) != EOF) {
    s_putc(s_w, ch);
  }

  s_close(s_r);
  s_close(s_w);

  return 0;
}
#endif /* TEST_STREAM */


