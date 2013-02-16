#define _GNU_SOURCE     1
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#ifndef __GLIBC__
/* In GLIBC, <string.h> will provide better basename(3). */
#include <libgen.h>
#endif

#include "xerror.h"

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <libgen.h>
#include <limits.h>
#endif

/* glibc compatible name */
const char *program_name __attribute__((weak)) = 0;

int debug_mode __attribute__((weak));

static void set_program_name(void) __attribute__((constructor));

#ifdef __APPLE__
static void
darwin_program_name(void)
{
  static char namebuf[PATH_MAX];
  uint32_t bufsize = PATH_MAX;
  int ret;

  ret = _NSGetExecutablePath(namebuf, &bufsize);
  if (ret == 0) {
    program_name = basename(namebuf);
  }
}
#endif  /* __APPLE__ */


static void
set_program_name(void)
{
#ifdef __APPLE__
  darwin_program_name();
#elif defined(__GLIBC__)
  program_name = basename(program_invocation_short_name);
#endif
}


void
xerror(int status, int code, const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  xmessage(1, code, format, ap);
  va_end(ap);

  if (status)
    exit(status);
}


void
xdebug_(int code, const char *format, ...)
{
  va_list ap;

  if (!debug_mode)
    return;

  va_start(ap, format);
  xmessage(0, code, format, ap);
  va_end(ap);
}


void
xmessage(int progname, int code, const char *format, va_list ap)
{
  char errbuf[BUFSIZ];

  fflush(stdout);
  fflush(stderr);

  flockfile(stderr);

  if (progname && program_name)
    fprintf(stderr, "%s: ", program_name);

  vfprintf(stderr, format, ap);

  if (code) {
    if (strerror_r(code, errbuf, BUFSIZ) == 0)
      fprintf(stderr, ": %s", errbuf);
  }
  fputc('\n', stderr);

  funlockfile(stderr);
}


#ifdef _TEST_XERROR
#include <errno.h>

int debug_mode = 1;

int
main(int argc, char *argv[])
{
  xdebug(0, "program_name = %s\n", program_name);

  if (argc != 2)
    xerror(1, 0, "argument required, argc = %d", argc);

  xerror(0, EINVAL, "invalid argv[1] = %s", argv[1]);
  return 0;
}

#endif  /* _TEST_XERROR */
