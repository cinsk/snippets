/* $Id$ */

#include <stdlib.h>
#include <string.h>

/*
 * `str_substitute' replaces every occurrence of NEEDLE in SRC to
 * REPLACE.
 *
 * Before using this function, you should place in `*SRC' the address
 * of a buffer `*SIZE' bytes long, allocated with `malloc'.  Note that
 * `*SIZE' is not the length of the source string, but the length of
 * the buffer.
 *
 * During substitution, if the source string *SRC is not enough large
 * to hold the result, `str_substitute' make `*SRC' bigger using
 * `realloc', and place the new size in `*SIZE'.
 *
 * `str_substitute' returns the number of substitution.  It returns -1
 * when the allocation failed.
 */
int
str_substitute(char **src, size_t *size,
               const char *needle, const char *replace)
{
  char *p, *s, *t;
  int len_rep, len_needle, len_src;
  size_t siz;
  int mul = 4;
  int count = 0;

  len_rep = strlen(replace);
  len_src = strlen(*src);
  len_needle = strlen(needle);
  siz = *size;

  s = *src;
  while ((p = strstr(s, needle)) != NULL) {
    if (*size < len_src + len_rep - len_needle + 1) {
      siz = siz + mul * (len_rep - len_needle);
      t = realloc(*src, siz);
      if (!t)
        return -1;
      p = p - *src + t;
      s = s - *src + t;
      *src = t;
      *size = siz;
      mul *= 2;
    }

    memmove(p + len_rep, p + len_needle,
            len_src - (p + len_needle - *src) + 1);
    memcpy(p, replace, len_rep);
    count++;
    s = p + len_rep;
    len_src = len_src + len_rep - len_needle;
  }
  return count;
}


int
main(void)
{
  char *s = "HELLO:$HOME|ASDF!$HOME%QWER";
  int len = strlen(s);

  char *src;
  size_t size = len + 1;

  src = strdup(s);
  //memset(src, '.', size);
  //strcpy(src, s);

  str_substitute(&src, &size, "$HOME", "oacewer");

  return 0;
}


