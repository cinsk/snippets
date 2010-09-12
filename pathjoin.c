/* pathjoin -- path join functions
 * Copyright (C) 2010  Seong-Kook Shin <cinsky@gmail.com>
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

/* $Id$ */
#include <stdarg.h>
#include <string.h>
#include "pathjoin.h"

#ifndef DIR_SEPARATOR
#define DIR_SEPARATOR '/'
#endif

#ifndef DIR_SEPARATORS
#define DIR_SEPARATORS "/"
#endif

static int path_join_va(char buf[], size_t bufsize,
                        va_list *ap, const char *argv[]);

static __inline__ void
path_elem_strip(const char *element, const char **start, const char **end)
{
  int len;
  const char *p = element;

  if (!p) {
    *start = *end = NULL;
    return;
  }

  len = strlen(p);
  if (len == 0) {
    *start = *end = p;
    return;
  }

  while (*p == DIR_SEPARATOR && *p != '\0')
    p++;

  if (*p == '\0') {
    *start = *end = p;
    return;
  }

  // "//a//"
  //    ^
  *start = p;
  p = element + len - 1;

  while (*p == DIR_SEPARATOR && *start < p)
    p--;

  *end = p + 1;
}


static __inline__ void
path_root_strip(const char *element, const char **start, const char **end)
{
  path_elem_strip(element, start, end);

  if (*start != element)
    *start = *start - 1;
}


int
path_join(char buf[], size_t bufsize, ...)
{
  va_list ap;
  int ret;

  va_start(ap, bufsize);
  ret = path_join_va(buf, bufsize, &ap, NULL);
  va_end(ap);
  return ret;
}


int
path_join_v(char buf[], size_t bufsize, const char *argv[])
{
  int ret;

  ret = path_join_va(buf, bufsize, NULL, argv);
  return ret;
}


static int
path_join_va(char buf[], size_t bufsize, va_list *ap, const char *argv[])
{
  long needed = 0;              /* Total bytes needed for successful job */
  size_t remaining = bufsize;   /* remaining bytes til the current line */
  size_t size;                  /* size of the path component */
  char *p = buf;                /* points the current adding point */
  const char *elem;
  const char *begin, *end = NULL;
  int i = 0;

  if (argv)
    elem = argv[i++];
  else
    elem = (const char *)va_arg(ap, const char *);
  if (!elem)
    return 0;

  path_root_strip(elem, &begin, &end);
  size = end - begin;
  if (size == 1 && *begin == DIR_SEPARATOR) {
    /* If the first element is just "/", we don't need to add it since
     * from the second element, in the following 'while' statement, it
     * will automatically added. */
    ;
  }
  else {
    needed += size;
    if (remaining >= size) {
      memcpy(p, begin, size);
      remaining -= size;
      p += size;
    }
    else {
      memcpy(p, begin, remaining);
      remaining = 0;
      p += remaining;
    }
  }

  while (1) {
    if (argv)
      elem = argv[i++];
    else
      elem = (const char *)va_arg(*ap, const char *);
    if (!elem)
      break;

    path_elem_strip(elem, &begin, &end);
    size = end - begin;

    if (size > 0) {
      /* Add separater before adding new element */
      if (remaining > 0) {
        *p++ = DIR_SEPARATOR;
        remaining--;
      }
      needed++;
    }

    needed += size;
    if (remaining >= size) {
      memcpy(p, begin, size);
      remaining -= size;
      p += size;
    }
    else {
      memcpy(p, begin, remaining);
      remaining = 0;
      p += remaining;
    }
  }

  /* If the last element of the path components does end with the
   * separator, it means that the result pathname is the name of a
   * directory.  In that case, I want to leave the last separator in
   * the result. */
  if (end != NULL && *end == DIR_SEPARATOR) {
    if (remaining > 0) {
      *p++ = DIR_SEPARATOR;
      remaining--;
    }
    needed++;
  }

  /* Now, add the '\0' to make a valid C string */
  if (remaining > 0) {
    *p++ = '\0';
    remaining--;
  }
  needed++;

  return needed;
}


#ifdef TEST_PATHJOIN
#include <stdio.h>

#define BUF_MAX 0

void
memdump(const char *begin, const char *end)
{
  while (begin != end)
    putchar(*begin++);
}

int
test_path_join(int argc, char *argv[])
{
  char buf[BUF_MAX];
  int ret;
  char *p = buf;

  p = NULL;
  ret = path_join_va(p, BUF_MAX, NULL, (const char **)(argv + 1));

  //ret = path_join(buf, BUF_MAX, "//asdf//", "///qwer/", "/zxcv", "///wert///", "sdfg", "", "dfgy///", "rety//", NULL);
  //ret = path_join(buf, BUF_MAX, "//asdf//", "///qwer/", NULL);

  if (p)
    printf("buf: %s\n", p);
  printf("ret: %d\n", ret);

  return 0;
}


int
test_path_elem_strip(int argc, char *argv[])
{
  const char *begin, *end;
  int i;

  if (argc == 1)
    return 1;

  path_root_strip(argv[1], &begin, &end);
  memdump(begin, end);
  putchar('\n');

  for (i = 2; i < argc; i++) {
    path_elem_strip(argv[i], &begin, &end);
    memdump(begin, end);
    putchar('\n');
  }


  return 0;
}

int
main(int argc, char *argv[])
{
  return test_path_join(argc, argv);
}

#endif  /* TEST_PATHJOIN */
