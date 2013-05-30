#define _GNU_SOURCE     1
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include <stdint.h>
#include <sys/time.h>

#define SEC_TIMESTAMP

#if (defined(SEC_TIMESTAMP) && defined(__Android__))
#include <time.h>
#endif

#ifndef NO_MCONTEXT
# ifdef __APPLE__
/* MacOSX deprecates <ucontext.h> */
#  include <sys/ucontext.h>
# else
#  include <ucontext.h>
# endif
#endif  /* NO_MCONTEXT */

#include <unistd.h>
#include <signal.h>
#include <execinfo.h>

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

#define BACKTRACE_MAX   16

const char *xbacktrace_executable __attribute__((weak)) = "/usr/bin/backtrace";

/* glibc compatible name */
const char *program_name __attribute__((weak)) = 0;

int debug_mode __attribute__((weak));
int backtrace_mode __attribute__((weak)) = 1;

static void set_program_name(void) __attribute__((constructor));

static FILE *xerror_stream = (FILE *)-1;

static void bt_handler(int signo, siginfo_t *info, void *uctx_void);
static void bt_handler_gdb(int signo, siginfo_t *info, void *uctx_void);

FILE *
xerror_redirect(FILE *fp)
{
  FILE *old = xerror_stream;

  assert(fp != NULL);

  if (old == (FILE *)-1)
    old = NULL;

  xerror_stream = fp;

  return old;
}


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

#if defined (SEC_TIMESTAMP)
unsigned int GetTickCount()
{
#if defined (__Android__)
  struct timespec now;
  int err = clock_gettime(CLOCK_MONOTONIC, &now);
  return now.tv_sec*1000000000LL + now.tv_nsec;
#else
  struct timeval gettick;
  unsigned int tick;

  gettimeofday(&gettick, NULL);

  tick = gettick.tv_sec*1000 + gettick.tv_usec/1000;

  return tick;
#endif
}
#endif


int
xbacktrace_on_signals(int signo, ...)
{
  struct sigaction act;
  va_list ap;

  int ret = 0;

  memset(&act, 0, sizeof(act));

  if (xbacktrace_executable && access(xbacktrace_executable, X_OK) == 0)
    act.sa_sigaction = bt_handler_gdb;
  else
    act.sa_sigaction = bt_handler;

  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_SIGINFO | SA_RESETHAND;

  ret = sigaction(signo, &act, NULL);
  if (ret != 0) {
    xerror(0, errno, "can't register a handler for signal %d", signo);
    return -1;
  }

  va_start(ap, signo);
  while ((signo = (int)va_arg(ap, int)) != 0) {
    ret = sigaction(signo, &act, NULL);
    if (ret != 0) {
      xerror(0, errno, "can't register a handler for signal %d", signo);
      va_end(ap);
      return -1;
    }
  }
  va_end(ap);
  return 0;
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


int
xifdebug()
{
  return debug_mode;
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
  int saved_errno = errno;

  if (xerror_stream == (FILE *)-1)
    xerror_stream = stderr;

  if (!xerror_stream)
    return;

  fflush(stdout);
  fflush(xerror_stream);

  flockfile(xerror_stream);

  if (progname && program_name)
    fprintf(xerror_stream, "%s: ", program_name);

#if defined(SEC_TIMESTAMP)
  fprintf(stderr, "%u, ", GetTickCount());
#endif

  vfprintf(xerror_stream, format, ap);

  if (code) {
#ifdef _GNU_SOURCE
    fprintf(xerror_stream, ": %s", strerror_r(code, errbuf, BUFSIZ));
#else
    /* We'll use XSI-compliant strerror_r() */
    errno = 0;
    if (strerror_r(code, errbuf, BUFSIZ) == 0)
      fprintf(xerror_stream, ": %s", errbuf);
    else if (errno == ERANGE)
      fprintf(xerror_stream, ": [xerror] invalid error code");
    else
      fprintf(xerror_stream, ": [xerror] strerror_r(3) failed (errno=%d)",
              errno);
#endif  /* _GNU_SOURCE */
  }

  fputc('\n', xerror_stream);

  funlockfile(xerror_stream);
  errno = saved_errno;
}


static void
bt_handler(int signo, siginfo_t *info, void *uctx_void)
{
  void *trace[BACKTRACE_MAX];
  int ret;

  (void)uctx_void;

  if (!backtrace_mode)
    return;

  {
#ifndef NO_MCONTEXT
# ifdef __APPLE__
    ucontext_t *uctx = (ucontext_t *)uctx_void;
    uint64_t pc = uctx->uc_mcontext->__ss.__rip;
    xerror(0, 0, "Got signal (%d) at address %8p, RIP=[%08llx]", signo,
           info->si_addr, pc);
# elif defined(REG_EIP) /* linux */
    ucontext_t *uctx = (ucontext_t *)uctx_void;
    greg_t pc = uctx->uc_mcontext.gregs[REG_EIP];
    xerror(0, 0, "Got signal (%d) at address [%0*lx], EIP=[%0*lx]", signo,
           sizeof(void *) * 2,
           (long)info->si_addr,
           sizeof(void *) * 2, (long)pc);
# elif defined(REG_RIP) /* linux */
    ucontext_t *uctx = (ucontext_t *)uctx_void;
    greg_t pc = uctx->uc_mcontext.gregs[REG_RIP];
    xerror(0, 0, "Got signal (%d) at address [%0*lx], RIP=[%0*lx]", signo,
           (int)(sizeof(void *) * 2),
           (long)info->si_addr,
           (int)(sizeof(void *) * 2), (long)pc);
# endif
#else
    xerror(0, 0, "Got signal (%d) at address %8p", signo,
           info->si_addr);
#endif  /* NO_MCONTEXT */
  }

  /* WARNING!
   *
   * None of functions that used below are async signal-safe.
   * Thus, this is not a portable code. -- cinsk
   */

  fprintf(xerror_stream, "\nBacktrace:\n");
  ret = backtrace(trace, BACKTRACE_MAX);
  /* TODO: error check on backtrace(3)? */

  fflush(xerror_stream);
  backtrace_symbols_fd(trace, ret, fileno(xerror_stream));
  fflush(xerror_stream);

  /* http://tldp.org/LDP/abs/html/exitcodes.html */
  exit(128 + signo);
}


static void
bt_handler_gdb(int signo, siginfo_t *info, void *uctx_void)
{
  char cmdbuf[LINE_MAX] = { 0, };

  snprintf(cmdbuf, LINE_MAX - 1, "backtrace %d", (int)getpid());
  system(cmdbuf);

}


#ifdef _TEST_XERROR
#include <errno.h>

int debug_mode = 1;

static void bar(int a)
{
  unsigned char *p = 0;
  *p = 3;                       /* SIGSEGV */
}

void foo(int a, int b)
{
  bar(a);
}


int
main(int argc, char *argv[])
{
  xbacktrace_on_signals(SIGSEGV, SIGILL, SIGFPE, SIGBUS, SIGTRAP, 0);

  xdebug(0, "program_name = %s\n", program_name);

  if (argc != 2)
    xerror(1, 0, "argument required, argc = %d", argc);

  xerror(0, EINVAL, "invalid argv[1] = %s", argv[1]);

  foo(1, 3);
  return 0;
}

#endif  /* _TEST_XERROR */
