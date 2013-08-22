#include <stdio.h>
#include <stdlib.h>
#include <printf.h>
#include <limits.h>
#include "xerror.h"

static void register_printf_b(void) __attribute__((constructor));

static int
b_handler(FILE *fp, const struct printf_info *info, const void *const *args)
{
  int len = 0;
  int width;
  char *output;
  unsigned long long value;
  int one_encountered = 0;
  int i;

  if (info->is_char) {
    width = sizeof(char) * CHAR_BIT;
    value = *(unsigned char *)(args[0]);
  }
  else if (info->is_short) {
    width = sizeof(short) * CHAR_BIT;
    value = *(unsigned short *)(args[0]);
  }
  else if (info->is_long) {
    width = sizeof(long) * CHAR_BIT;
    value = *(unsigned long *)(args[0]);
  }
  else if (info->is_long_double) {
    width = sizeof(long long) * CHAR_BIT;
    value = *(unsigned long long *)(args[0]);
  }
  else {
    width = sizeof(int) * CHAR_BIT;
    value = *(unsigned int *)(args[0]);
  }
  //fprintf(stderr, "width: %d\n", width);
  //fprintf(stderr, "info width: %d\n", info->width);

  output = alloca(width);
  for (i = 0; i < width; i++) {
    output[i] = (value & 1) ? '1' : '0';
    value >>= 1;
  }
  for (i = 0; i < width / 2; i++) {
    char t = output[i];
    output[i] = output[width - 1 - i];
    output[width - 1 - i] = t;
  }

  flockfile(fp);
  if (info->width > width)
    for (i = 0; i < info->width - width; i++) {
      fputc_unlocked(info->pad, fp);
      len++;
    }

  for (i = 0; i < width; i++) {
    if (output[i] == '1') {
      one_encountered = 1;
      fputc_unlocked(output[i], fp);
    }
    else if (output[i] == '0') {
      if (!one_encountered)
        fputc_unlocked(info->pad, fp);
      else
        fputc_unlocked(output[i], fp);
    }
    len++;
  }
  funlockfile(fp);
  return len;
}


static int
b_arginfo(const struct printf_info *info, size_t n, int *argtypes)
{
  if (n > 0)
    argtypes[0] = PA_INT;

  if (info->is_char)
    argtypes[0] = PA_CHAR;
  else if (info->is_short)
    argtypes[0] |= PA_FLAG_SHORT;
  else if (info->is_long)
    argtypes[0] |= PA_FLAG_LONG;
  else if (info->is_long_double)
    argtypes[0] |= PA_FLAG_LONG_LONG;

  return 1;
}


static void
register_printf_b(void)
{
  register_printf_function('b', b_handler, b_arginfo);
}


int
main(void)
{
  printf("%0b\n", 0xbeef);

  return 0;
}
