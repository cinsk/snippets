/*
 * leb128.c: Encode/Decode LEB128 number
 */

/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef LINE_MAX
#define LINE_MAX        256
#endif

/*
 * output_[su]leb128() functions are stolen from read.c in binutils-2.18
 * read_leb128() function is stolen from dwarf.c in binutils-2.18
 */

typedef long offsetT;
typedef unsigned long valueT;

int output_leb128 (char *p, valueT value, int sign);
unsigned long int read_leb128 (unsigned char *data,
                               unsigned int *length_return, int sign);
static void dump(const char *buf);
static inline int output_sleb128 (char *p, offsetT value);
static inline int output_uleb128 (char *p, valueT value);

int option_encode = 0;
int option_sign = 0;

static void help_and_exit(void);

int
main(int argc, char *argv[])
{
  char buf[LINE_MAX];
  int opt;
  char *endptr;
  long off;
  int len, i;
  unsigned int l;
  unsigned long val;

  while ((opt = getopt(argc, argv, "hsde")) != -1) {
    switch (opt) {
    case 'd':
      option_encode = 0;
      break;
    case 'e':
      option_encode = 1;
      break;
    case 's':
      option_sign = 1;
      break;
    case 'h':
      help_and_exit();
      break;
    default:
      fprintf(stderr, "Try `-h' for more help.\n");
      return 1;
    }
  }

  if (optind == argc) {
    fprintf(stderr, "error: argument required.\n");
    fprintf(stderr, "Try `-h' for more help.\n");
    return 1;
  }

  for (i = optind; i < argc; i++) {
    if (option_encode) {
      off = strtol(argv[i], &endptr, 0);
      if (endptr != NULL && *endptr == '\0') {
        len = output_leb128(buf, off, option_sign);
        buf[len] = '\0';

        dump(buf);
      }
    }
    else {
      off = strtol(argv[i], &endptr, 0);
      if (endptr != NULL && *endptr == '\0') {
        len = 0;
        val = read_leb128((unsigned char *)&off, &l, option_sign);

        printf("0x%08lX ", val);

        if (option_sign)
          printf("(%ld)\n", val);
        else
          printf("(%lu)\n", val);
      }
    }
  }

  return 0;
}


static void
help_and_exit(void)
{
  static const char *msgs[] = {
    "usage: leb128 [OPTION...] NUMBER...",
    "",
    "  -d     Decode LEB128 NUMBER",
    "  -e     Encode LEB128 number from NUMBER",
    "  -s     Force signed encode/decode",
    "  -h     Show this help messages",
    "",
  };
  int i;

  for (i = 0; i < sizeof(msgs) / sizeof(char *); i++)
    puts(msgs[i]);

  exit(0);
}


static inline int
output_sleb128 (char *p, offsetT value)
{
  register char *orig = p;
  register int more;

  do
    {
      unsigned byte = (value & 0x7f);

      /* Sadly, we cannot rely on typical arithmetic right shift behaviour.
         Fortunately, we can structure things so that the extra work reduces
         to a noop on systems that do things "properly".  */
      value = (value >> 7) | ~(-(offsetT)1 >> 7);

      more = !((((value == 0) && ((byte & 0x40) == 0))
                || ((value == -1) && ((byte & 0x40) != 0))));
      if (more)
        byte |= 0x80;

      *p++ = byte;
    }
  while (more);

  return p - orig;
}


static inline int
output_uleb128 (char *p, valueT value)
{
  char *orig = p;

  do
    {
      unsigned byte = (value & 0x7f);
      value >>= 7;
      if (value != 0)
        /* More bytes to follow.  */
        byte |= 0x80;

      *p++ = byte;
    }
  while (value != 0);

  return p - orig;
}


int
output_leb128 (char *p, valueT value, int sign)
{
  if (sign)
    return output_sleb128 (p, (offsetT) value);
  else
    return output_uleb128 (p, value);
}


unsigned long int
read_leb128 (unsigned char *data, unsigned int *length_return, int sign)
{
  unsigned long int result = 0;
  unsigned int num_read = 0;
  unsigned int shift = 0;
  unsigned char byte;

  do
    {
      byte = *data++;
      num_read++;

      result |= ((unsigned long int) (byte & 0x7f)) << shift;

      shift += 7;

    }
  while (byte & 0x80);

  if (length_return != NULL)
    *length_return = num_read;

  if (sign && (shift < 8 * sizeof (result)) && (byte & 0x40))
    result |= -1L << shift;

  return result;
}


static void
dump(const char *buf)
{
  printf("0x");
  while (*buf) {
    printf("%02X", *(unsigned char *)buf);
    buf++;
  }
  putchar('\n');
}
