/* $Id$ */
/*
 * snprintx: snprintf-like functions for easy formatting strings
 * Copyright (C) 2009  Seong-Kook Shin <cinsky@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdio.h>
#include <stdarg.h>
#include "snprintx.h"

int
vsnprintx(char **buf, ssize_t *size, const char *format, va_list ap)
{
  int ret;
  size_t sz;

  sz = (*size < 0) ? 0 : *size;

  ret = vsnprintf(*buf, sz, format, ap);

  if (ret < *size) {
    *buf = *buf + ret;
    *size = *size - ret;
    ret = 0;
  }
  else {
    *buf = *buf + *size;
    *size = -1;
  }
  return ret;
}


int
snprintx(char **buf, ssize_t *size, const char *format, ...)
{
  va_list ap;
  int ret;

  va_start(ap, format);
  ret = vsnprintx(buf, size, format, ap);
  va_end(ap);

  return ret;
}


#ifdef TEST_SNPRINTX
#include <string.h>

#define BUF_SIZE        10

int
main(int argc, char *argv[])
{
  char buf[BUF_SIZE];
  char *ptr = buf;
  ssize_t size = BUF_SIZE;
  int ret;

  if (1) {
    ret = snprintx(&ptr, &size, "h");
    ret = snprintx(&ptr, &size, "i");
    ret = snprintx(&ptr, &size, "!");
  }
  else
    ret = snprintx(&ptr, &size, "!!");

  printf("ret: %d\n", ret);
  printf("size: (%d)\n", size);
  printf("sizeof(buf) - size = (%d)\n", sizeof(buf) - size);
  printf("|%s| strlen(%d)\n", buf, strlen(buf) + 1);
  write(1, buf, sizeof(buf) - size);
  write(1, "\n", 1);

  return 0;
}
#endif  /* TEST_SNPRINTX */
