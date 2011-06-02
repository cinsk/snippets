/* $Id: easyconv.c,v 1.3 2010/06/27 23:45:28 cinsk Exp $ */
/* easy interface for GNU iconv module.
 * Copyright (C) 2009  Seong-Kook Shin <cinsky@gmail.com>
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

/*
 * TODO: add Fail-safe converting, such that if some of the bytes in
 *       the source could not be converted into the destination encoding,
 *       add some bytes ('.') and ignore them without converting error.
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "easyconv.h"

#ifdef EASYCONV_VERBOSE
#include <stdio.h>
#include <ctype.h>
#endif

size_t
easyconv_str2str(iconv_t CD, char **DST, size_t *DST_SIZE,
                 const char *SRC, size_t SRC_SIZE, size_t *NREVCONV)
{
  char *dst, *src;
  size_t dsize, ssize, conv;
  int i, olderr = 0;

  assert(DST != NULL);
  assert(DST_SIZE != NULL);

  if (SRC_SIZE == (size_t)-1)
    SRC_SIZE = (SRC) ? strlen(SRC) : 0;

  if (*DST == NULL) {
    int len = (*DST_SIZE) ? *DST_SIZE : EASYCONV_CHUNK_SIZE;
    *DST = malloc(len);
    if (!*DST)
      return (size_t)-1;
    //memset(*DST, 0xCC, len);
    *DST_SIZE = len;
  }

 retry:
  dst = *DST;
  dsize = *DST_SIZE;
  src = (char *)SRC;
  ssize = SRC_SIZE;

  iconv(CD, NULL, NULL, &src, &ssize);

  errno = 0;
  conv = iconv(CD, &src, &ssize, &dst, &dsize);
  if (NREVCONV)
    *NREVCONV = conv;
  olderr = errno;

  if (conv == (size_t)-1) {
    if (errno == E2BIG) {
      int len;
      if (*DST_SIZE < EASYCONV_CHUNK_SIZE)
        len = EASYCONV_CHUNK_SIZE;
      else
        len = (*DST_SIZE) * 2;

      *DST = realloc(*DST, len);
      if (!*DST)
        return (size_t)-1;
      *DST_SIZE = len;
      goto retry;
    }
    else if (errno == EILSEQ) {
#ifdef EASYCONV_VERBOSE
      int i;
      int len = (ssize > 16) ? 16 : ssize;

      fprintf(stderr, "error: iconv failed(EILSEQ): ");
      for (i = 0; i < len; i++) {
        unsigned char ch = src[i];
        if (isprint(ch))
          fprintf(stderr, "%c", ch);
        else
          fprintf(stderr, "\\x%02X", ch);
      }
      fputc('\n', stderr);
#endif  /* EASYCONV_VERBOSE */
    }
    else if (errno != EINVAL)
      return (size_t)-1;

    /* If you want to get the error report of EILSEQ, return
     * (size_t)-1 here.  If you do, the caller may not use the
     * contents in *DST since it does not end with the EOL. */
  }

  {
    /* TODO: Need to change to add the nul suffix into DST?? */
    if (dsize < 4) {
      /* We don't have enough space for the EOL. */
      int len = *DST_SIZE + 4;
      int off = dst - *DST;
      *DST = realloc(*DST, len);
      if (!*DST)
        return (size_t)-1;
      *DST_SIZE = len;
      dst = *DST + off;
    }

    /* Terminate it with the EOL. */
    for (i = 0; i < 4; i++)
      *dst++ = '\0';
  }

  errno = olderr;

  return dst - *DST;
}


#ifdef TEST_EASYCONV
#include <stdio.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
  iconv_t cd;
  char *converted;
  size_t size, nrevconv, ret;

  if (argc != 3) {
    fprintf(stderr, "usage: %s ENCODING TEXT\n", argv[0]);
    return 1;
  }

  //cd = iconv_open(argv[1], "UTF-8");
  cd = iconv_open("UTF-32", argv[1]);
  if (cd == (iconv_t)-1) {
    fprintf(stderr, "error: iconv_open() failed: %s", strerror(errno));
    return 1;
  }

#if 1
  converted = NULL;
  size = 0;
#else
  converted = malloc(1);
  *converted = 1;
  size = 1;
#endif

  ret = easyconv_str2str(cd, &converted, &size, argv[2], (size_t)-1, &nrevconv);
  if (ret != (size_t)-1) {
    fprintf(stderr, "%d byte(s) converted\n", ret);
    fprintf(stderr, "%d character(s) converted\n", nrevconv);
    write(STDOUT_FILENO, converted, ret);
  }
  else
    fprintf(stderr, "error: easyconv_str2str failed: %s\n", strerror(errno));
  free(converted);

  iconv_close(cd);

  return 0;
}
#endif  /* TEST_EASYCONV */
