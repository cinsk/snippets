
/*
 * Enhanced assert(3)
 * Copyright (C) 2003, 2004  Seong-Kook Shin <cinsky@gmail.com>
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <xassert.h>


void
assert_(const char *file, long line, const char *func,
        const char *expr, const char *format, ...)
{
  va_list ap;

  fflush(stdout);
  fflush(stderr);               /* possibly redundant */
  fprintf(stderr, "%s:%ld: Assertion `%s' failed at %s.\n\t",
          file, line, expr, func);
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fputc('\n', stderr);
  fflush(stderr);               /* possibly redundant */
  abort();
}


#ifdef TEST_XASSERT
int
main(void)
{
  int age = 1;
  ASSERT(age > 10, "Age should be larger than 10. (age = %d)", age);
  return 0;
}
#endif /* TEST_XASSERT */
