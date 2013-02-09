#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>

#include <unistd.h>
#include <getopt.h>
#include <error.h>

#define DEFAULT_INDENT  2
#define DEFAULT_NFIELD  4
#define DEFAULT_NCHAR   8

#define F_OCT   "0%03o, "
#define F_DEC   "%3u, "
#define F_HEX   "0x%02x, "

#define OFF_F_OCT "/* %0*lo */ "
#define OFF_F_DEC "/* %0*lu */ "
#define OFF_F_HEX "/* 0x%0*lX */ "

int gen_struct(FILE *out, FILE *in, const char *ident, size_t nfield);
unsigned long dump(FILE *out, FILE *in, const char *ident, size_t nfield);
FILE *xfopen_write(const char *pathname);
char *basename(const char *pathname);
void indent(FILE *out);
int fsize_digit_count(FILE *fp);
char *xstrncpy(char *d, const char *s, size_t size);

const char *format_data(int base);
const char *format_offset(int base);

void help_and_exit(void);
void version_and_exit(void);

FILE *target_stream;

char *ident_template;
int force_overwrite;
int use_static;
int use_const;
int use_tab;
int append_mode;
int digit_nfield = DEFAULT_NFIELD;
int show_offset = 1;
int indent_spaces = 2;
int char_per_line = 5;
int base_offset = 10;
int base_data = 16;

struct option longopts[] = {
  { "help", no_argument, 0, 1 },
  { "version", no_argument, 0, 2 },
  { "output", required_argument, 0, 'o' },
  { "offset-base", required_argument, 0, 'B' },
  { "base", required_argument, 0, 'b' },
  { "char-per-line", required_argument, 0, 'n' },
  { "show-comment", no_argument, 0, 'm' },
  { "no-comment", no_argument, 0, 'M' },
  { "use-static", no_argument, 0, 's' },
  { "no-static", no_argument, 0, 'S' },
  { "use-const", no_argument, 0, 'c' },
  { "no-const", no_argument, 0, 'C' },
  { "use-tab", no_argument, 0, 't' },
  { "no-tab", no_argument, 0, 'T' },
  { "indent-nchar", required_argument, 0, 'i' },
  { 0, 0, 0, 0 },
};

static const char *program_name = "bin2c";
static const char *version_string = "0.1";

int
main(int argc, char *argv[])
{
  FILE *src;
  int optch, i;
  char ident[LINE_MAX] = { 0, };

  while (1) {
    optch = getopt_long(argc, argv, "o:b:B:n:aAtTcCsSi:I:mM", longopts, 0);
    if (optch == -1)
      break;

    switch (optch) {
    case 1:
      help_and_exit();
      break;
    case 2:
      version_and_exit();
      break;
    case 's':
      use_static = 1;
      break;
    case 'S':
      use_static = 0;
      break;
    case 'm':
      show_offset = 1;
      break;
    case 'M':
      show_offset = 0;
      break;
    case 'c':
      use_const = 1;
      break;
    case 'C':
      use_const = 0;

    case 'a':
      append_mode = 1;
      break;
    case 'A':
      append_mode = 0;
      break;
    case 't':
      use_tab = 1;
      break;
    case 'T':
      use_tab = 0;
      break;

    case 'i':
      ident_template = strdup(optarg);
      break;

    case 'I':
      indent_spaces = atoi(optarg);
      if (indent_spaces <= 0)
        indent_spaces = DEFAULT_INDENT;
      use_tab = 0;
      break;

    case 'n':
      char_per_line = atoi(optarg);
      if (char_per_line <= 0)
        char_per_line = DEFAULT_NCHAR;
      break;
    case 'b':
      base_data = atoi(optarg);
      break;
    case 'B':
      base_offset = atoi(optarg);
      break;
    case 'o':
      target_stream = xfopen_write(optarg);
      if (!target_stream)
        error(1, errno, "cannot open %s", optarg);
      break;

    case '?':
    default:
      error(1, 0, "use `bin2c --help' for more information");
      break;
    }
  }
  if (!target_stream)
    target_stream = stdout;

  if (argc == optind) {
    /* no argument */
    error(1, 0, "use `bin2c --help' for more information");
    return 1;
  }

  for (i = optind; i < argc; i++) {
    if (strcmp(argv[i], "-") == 0)
      src = stdin;
    else {
      src = fopen(argv[i], "rb");
      if (!src) {
        error(0, errno, "cannot open %s", argv[i]);
        continue;
      }
    }

    if (ident_template) {
      if (optind == argc - 1)
        snprintf(ident, LINE_MAX, "%s", ident_template);
      else
        snprintf(ident, LINE_MAX, "%s%d", ident_template, i - optind);
    }
    else
      xstrncpy(ident, basename(argv[i]), LINE_MAX);

    gen_struct(target_stream, src, ident, digit_nfield);

    if (src != stdin)
      fclose(src);
  }

  return 0;
}


int
gen_struct(FILE *out, FILE *in, const char *ident, size_t nfield)
{
  int i;
  int firstch = 1;
  unsigned long written;

  if (use_static)
    fprintf(out, "static ");

  if (use_const)
    fprintf(out, "const ");

  fprintf(out, "char ");

  if (in == stdin && ident == 0)
    ident = "stdin_dump";

  if (isdigit(ident[0]))
    fprintf(out, "_%s[] = {\n", ident);
  else
    fprintf(out, "%s[] = {\n", ident);

  written = dump(out, in, ident, digit_nfield);

  fprintf(out, "};\n");
  fprintf(out, "#define ");

  for (i = 0; ident[i] != '\0'; i++) {
    if (firstch && isdigit(ident[i]))
      fputc('_', out);
    firstch = 0;

    if (isalnum(ident[i]))
      fputc(toupper(ident[i]), out);
    else
      fputc('_', out);
  }

  fprintf(out, "_LEN\t%lu\n\n", written);
  return 0;
}


unsigned long
dump(FILE *out, FILE *in, const char *ident, size_t nfield)
{
  unsigned long written = 0;
  int ch;
  unsigned char c;
  const char *format, *off_format;

  format = format_data(base_data);
  off_format = format_offset(base_offset);

  if (nfield == 0) {
    if (in != stdin)
      nfield = fsize_digit_count(in);
    else
      nfield = DEFAULT_NFIELD;
  }

  while ((ch = fgetc(in)) != EOF) {
    if (written % char_per_line == 0) {
      indent(out);
      if (show_offset)
        fprintf(out, off_format, nfield, written);
    }

    c = (unsigned char)ch;
    fprintf(out, format, c);
    if (written % char_per_line == char_per_line - 1) {
      fprintf(out, "\n");
    }
    written++;
  }
  if (written % char_per_line != 0) {
    fprintf(out, "\n");
  }
  return written;
}


FILE *
xfopen_write(const char *pathname)
{
  FILE *fp;
  assert(pathname != 0);

  if (access(pathname, F_OK) == 0 && !force_overwrite)
    return 0;

  fp = fopen(pathname, append_mode ? "a" : "w");
  return fp;
}


char *
basename(const char *pathname)
{
  char *p;

  assert(pathname != 0);

  p = strrchr(pathname, '/');
  if (!p)
    p = (char *)pathname;
  else
    p++;
  return p;
}


void
indent(FILE *out)
{
  int i;

  if (use_tab)
    fputc('\t', out);
  else {
    for (i = 0; i < indent_spaces; i++)
      fputc(' ', out);
  }
}


int
fsize_digit_count(FILE *fp)
{
  int count;
  long pos;

  pos = ftell(fp);

  rewind(fp);
  fseek(fp, 0, SEEK_END);
  count = snprintf(0, 0, "%lu", (unsigned long)ftell(fp));

  fseek(fp, pos, SEEK_SET);
  if (count < 4)
    count = 4;
  return count;
}


const char *
format_data(int base)
{
  switch (base) {
  case 8:
    return F_OCT;
  case 10:
    return F_DEC;
  case 16:
  default:
    return F_HEX;
  }
}


const char *
format_offset(int base)
{
  switch (base) {
  case 8:
    return OFF_F_OCT;
  case 16:
    return OFF_F_HEX;
  case 10:
  default:
    return OFF_F_DEC;
  }
}


void
help_and_exit(void)
{
  static const char *msgs[] = {
    "Create C/C++ char array using binary data",
    "usage: bin2c [OPTION...] [FILE...]",
    "",
    "  -i, --ident=ID                use ID as the array identifier",
    "  -n, --char-per-line=CHARS     override the number of chars in a line",
    "  -o, --output=FILE             override the output to FILE",
    "",
    "  -b, --base=NUM                use base NUM digits to print data",
    "  -B, --offset-base=NUM         use base NUM digits to print the offset",
    "",
    "  -m, --show-comment            add offset comments each line",
    "  -M, --no-comment              no offset comments each line",
    "  -c, --use-const               add `const' qualifier",
    "  -C, --no-const                remove `const' qualifier",
    "  -s, --use-static              add `static' storage specifier",
    "  -S, --no-static               remove `static' storage specifier",
    "",
    "  -t, --use-tab                 use tab character to indent lines",
    "  -T, --no-tab                  use space character to indent lines",
    "  -I, --indent-nchar=NUM        use NUM spaces to indent lines",
    "",
    "      --version                 output version information and exit",
    "      --help                    display this help and exit",
    "",
    "By default, The output is set to STDOUT, use `-o' to override this.",
    "To use STDIN as the input, use `-' as the filename",
    "",
    "Report bugs to <cinsky@gmail.com>"
  };
  int i;

  for (i = 0; i < (int)sizeof(msgs) / sizeof(const char *); i++)
    puts(msgs[i]);

  exit(0);
}


void
version_and_exit(void)
{
  printf("bin2c version %s\n", version_string);
  exit(0);
}


void
error(int status, int ecode, const char *format, ...)
{
  va_list argptr;

  fflush(stdout);
  fflush(stderr);

  if (program_name)
    fprintf(stderr, "%s: ", program_name);

  if (ecode)
    fprintf(stderr, "%s: ", strerror(ecode));

  va_start(argptr, format);
  vfprintf(stderr, format, argptr);
  va_end(argptr);
  fputc('\n', stderr);
  fflush(stderr);

  if (status)
    exit(status);
}


char *
xstrncpy(char *d, const char *s, size_t size)
{
  char *p, *q;

  q = d;
  for (p = (char *)s; p - s < size - 1; p++)
    *q++ = *p;

  *q = '\0';
  return q;
}
