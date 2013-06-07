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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fnmatch.h>

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
#define IGNORE_ENVNAME  "XERROR_IGNORES"
#define IGNORE_FILENAME ".xerrignore"

const char *xbacktrace_executable __attribute__((weak)) = "/usr/bin/backtrace";

/* glibc compatible name */
const char *program_name __attribute__((weak)) = 0;

int debug_mode __attribute__((weak));
int backtrace_mode __attribute__((weak)) = 1;

static void set_program_name(void) __attribute__((constructor));

static FILE *xerror_stream = (FILE *)-1;

static void bt_handler(int signo, siginfo_t *info, void *uctx_void);
static void bt_handler_gdb(int signo, siginfo_t *info, void *uctx_void);


static int ign_reserve(void);
static int ign_load(const char *basedir);
static int ign_load_file(const char *pathname);
static int ign_match(const char *src);

static void ign_free(void) __attribute__((destructor));

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


int
xbacktrace_on_signals(int signo, ...)
{
  struct sigaction act;
  va_list ap;

  int ret = 0;

  memset(&act, 0, sizeof(act));

  if (xbacktrace_executable && access(xbacktrace_executable, X_OK) == 0 &&
      getenv("XBACKTRACE_NOGDB") == 0)
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
  xmessage(1, code, 0, format, ap);
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
  xmessage(0, code, 1, format, ap);
  va_end(ap);
}


void
xmessage(int progname, int code, int ignore, const char *format, va_list ap)
{
  char errbuf[BUFSIZ];
  int saved_errno = errno;

  if (xerror_stream == (FILE *)-1)
    xerror_stream = stderr;

  if (!xerror_stream)
    return;

  if (ignore) {
    va_list vcp;
    int pred;

    va_copy(vcp, ap);
    pred = ign_match((const char *)va_arg(vcp, const char *));
    va_end(vcp);

    if (pred)
      return;
  }

  fflush(stdout);
  fflush(xerror_stream);

  flockfile(xerror_stream);

  if (progname && program_name)
    fprintf(xerror_stream, "%s: ", program_name);

  vfprintf(xerror_stream, format, ap);

  if (code) {
#if defined(_GNU_SOURCE) && !defined(__APPLE__)
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
  static char cmdbuf[LINE_MAX] = { 0, };
  static char cwd[PATH_MAX];

  char *file = getenv("XERROR_BACKTRACE_FILE");

  if (file)
    snprintf(cmdbuf, LINE_MAX - 1, "backtrace %d %s.%d",
             (int)getpid(), file, (int)getpid());
  else {
    if (getcwd(cwd, PATH_MAX) == 0)
      cwd[0] = '\0';

    if (getppid() == 1 || strcmp(cwd, "/") == 0) {
      if (access("/var/log", W_OK) == 0)
        snprintf(cmdbuf, LINE_MAX - 1, "backtrace %d /var/log/gdb.%d",
                 (int)getpid(), (int)getpid());
      else
        snprintf(cmdbuf, LINE_MAX - 1, "backtrace %d /tmp/gdb.%d",
                 (int)getpid(), (int)getpid());
    }
    else
      snprintf(cmdbuf, LINE_MAX - 1, "backtrace %d", (int)getpid());
  }

  system(cmdbuf);
}


static struct {
  size_t cap;
  size_t cur;
  char **pat;
} ignore = { 0, 0, 0 };


static int
ign_reserve(void)
{
  void *p;

  if (ignore.cur >= ignore.cap) {
    if (!ignore.cap)
      ignore.cap = 32;
    ignore.cap *= 2;
    p = realloc(ignore.pat, ignore.cap);
    if (!p)
      return -1;

    ignore.pat = p;
  }
  return 0;
}


static int
ign_load_file(const char *pathname)
{
  FILE *fp;
  char *line = 0;
  size_t lnsize = 0;
  ssize_t len;

  fp = fopen(pathname, "r");
  if (!fp)
    return -1;

  while ((len = getline(&line, &lnsize, fp)) != -1) {
    if (line[len - 1] == '\n')
      line[len - 1] = '\0';
    if (line[0] == '\0' || line[0] == '#')
      continue;

    if (ign_reserve() == -1) {
      /* TODO: error handling */
      break;
    }

    ignore.pat[ignore.cur++] = strdup(line);
  }
  free(line);
  fclose(fp);
}


static int
ign_load(const char *basedir)
{
  const char *env = getenv(IGNORE_ENVNAME);
  int cwdfd;
  char cwdbuf[PATH_MAX], *cwd;

  if (env && ign_load_file(env) == 0)
    return 0;

  cwdfd = open(".", O_RDONLY);
  if (cwdfd == -1)
    return -1;

  while (1) {
    if (ign_load_file(IGNORE_FILENAME) == 0)
      break;

    cwd = getcwd(cwdbuf, PATH_MAX);
    if (cwd && strcmp(cwd, "/") == 0)
      break;

    if (chdir("..") == -1)
      break;
  }

  fchdir(cwdfd);
  close(cwdfd);
  return 0;
}


static int
ign_match(const char *src)
{
  size_t i;

  for (i = 0; i < ignore.cur; i++) {
    if (fnmatch(ignore.pat[i], src, FNM_FILE_NAME | FNM_LEADING_DIR) == 0)
      return 1;
  }
  return 0;
}


static void
ign_free(void)
{
  size_t i;
  char **pat = ignore.pat;
  size_t cur = ignore.cur;

  ignore.cur = 0;
  ignore.pat = 0;
  ignore.cap = 0;

  for (i = 0; i < cur; i++)
    free(pat[i]);

  free(pat);
}


int
xerror_init(const char *prog_name, const char *ignore_search_dir)
{
  if (prog_name)
    program_name = prog_name;

  ign_load(ignore_search_dir);
  return 0;
}


#if 0
static int
load_ignore()
{
  int cwd_fd;
  FILE *fp;

  cwd_fd = open(".", O_RDONLY);
  if (cwd_fd == -1)
    return -1;

  while (1) {
    fp = fopen(".xerrignore", "r");
    if (!fp) {
      if (chdir("..") == -1)



  if (fchdir(cwd_fd) == -1) {
    fprintf(stderr, "warning: xerror can't revert the CWD.\n");
    return -1;
  }
}
#endif  /* 0 */

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
  xerror_init(0, 0);

  xbacktrace_on_signals(SIGSEGV, SIGILL, SIGFPE, SIGBUS, SIGTRAP, 0);

  xdebug(0, "program_name = %s", program_name);
  xdebug(0, "this is debug message %d", 1);

  if (argc != 2)
    xerror(1, 0, "argument required, argc = %d", argc);

  xerror(0, EINVAL, "invalid argv[1] = %s", argv[1]);

  foo(1, 3);
  return 0;
}

#endif  /* _TEST_XERROR */
