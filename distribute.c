#define _GNU_SOURCE

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <libgen.h>
#include <getopt.h>

#ifdef __APPLE__
#include <err.h>
#define error(s, e, ...)        errc((s), (e), __VA_ARGS__)
#else
#include <error.h>
#endif

#ifndef FALSE
#define FALSE   0
#define TRUE    (!FALSE)
#endif

#define P_READ  0
#define P_WRITE 1

#ifndef LINE_MAX
#define LINE_MAX        2048
#endif

#define SIGBUF_MAX      64

#define VERSION_STRING  "0.1"

static struct option longopts[] = {
  { "jobs", required_argument, NULL, 'j' },
  { "verbose", no_argument, NULL, 'v' },
  { "out-template", required_argument, NULL, 'O' },
  { "err-template", required_argument, NULL, 'E' },
  { "input", required_argument, NULL, 'i' },
  { "help", no_argument, NULL, 1 },
  { "version", no_argument, NULL, 2 },
  { NULL, 0, NULL, 0 },
};


struct proc {
  pid_t pid;
  int status;
  int fds[3];                   /* [0] for child STDIN,
                                 * [1] for child STDOUT if redirected,
                                 * [2] for child STDERR if redirected.
                                 */
};


struct {
  FILE *in;

  char *out_tmpl;               /* e.g. "/tmp/distribute.XXXXXX" */
  char *err_tmpl;

  //int out[2];                   /* 1 for STDOUT, 0 for STDERR */

  int verbose;
  int debug;

  int redirect_out;
  int redirect_err;

  int sig_fds[2];
  int njobs;

  struct proc *procs;

  struct pollfd *fds;
  size_t nfds;

  short poll_errmask;
  sigset_t sigmask;
  sigset_t poll_sigmask;

} env;


int set_closexec(int fd);
int set_nonblock(int fd);
int getncores(void);


static void msgv_out(int status, int ecode, const char *fmt, va_list ap);
void debug(int ecode, const char *fmt, ...)
  __attribute__((format(printf, 2, 3)));
void message(int ecode, const char *fmt, ...)
  __attribute__((format(printf, 2, 3)));

#ifdef __APPLE__
#if 0
void error(int status, int ecode, const char *fmt, ...)
  __attribute__((format(printf, 3, 4)));
#endif  /* 0 */
#endif

static const char *program_name = "distribute";

static void init_env(void);
static void set_poll_errmask();
static void reap_children(int wait);
static int has_valid_fd(struct pollfd *fds, nfds_t nfds);
static int start_server(void);
static int cleanup_server(void);
static void prepare_signals(void);
static struct proc *spawn_children(size_t nproc, int argc, char *argv[]);
static struct pollfd *prepare_poll(size_t *nfds, int sigfd,
                                   struct proc *procs, size_t njobs);
static void child_main(int argc, char *argv[]);
static void version_and_exit();
static void usage(void);


int
main(int argc, char *argv[])
{
  int opt;

  //program_name = basename(argv[0]);
  set_poll_errmask();

  init_env();

  while ((opt = getopt_long(argc, argv, "j:i:O:E:vD", longopts, NULL)) != -1) {
    switch (opt) {
    case 1:
      usage();
      break;
    case 2:
      version_and_exit();
      break;
    case 'v':
      env.verbose = TRUE;
      break;
    case 'D':
      env.debug = TRUE;
      break;
    case 'i':
      if (strcmp(optarg, "-") == 0)
        env.in = stdin;
      else {
        env.in = fopen(optarg, "r");
        if (!env.in)
          error(1, errno, "cannot open input file, %s", optarg);
        set_closexec(fileno(env.in));
      }
      break;
    case 'O':
      env.redirect_out = TRUE;

      asprintf(&env.out_tmpl, "%sXXXXXX", optarg);
      break;
    case 'E':
      env.redirect_err = TRUE;

      asprintf(&env.err_tmpl, "%sXXXXXX", optarg);
      break;

    case 'j':
      env.njobs = atoi(optarg);
      break;

    default:
      printf("Try `--help' for more.\n");
      exit(1);
    }
  }
  argc -= optind;
  argv += optind;

  prepare_signals();

  message(0, "using %d job process(s)", env.njobs);

  env.procs = spawn_children(env.njobs, argc, argv);
  start_server();
  cleanup_server();

  return 0;
}


static void
init_env(void)
{
  env.in = stdin;

  env.out_tmpl = NULL;
  env.err_tmpl = NULL;

  env.verbose = FALSE;
  env.debug = FALSE;

  env.redirect_out = FALSE;
  env.redirect_err = FALSE;

  env.njobs = getncores();
  env.procs = NULL;
  env.fds = NULL;
  env.nfds = 0;
}


static void
set_poll_errmask()
{
  env.poll_errmask = POLLERR | POLLHUP | POLLNVAL;

#ifdef POLLRDHUP
  env.poll_errmask += POLLRDHUP;
#endif
}


static int
has_valid_fd(struct pollfd *fds, nfds_t nfds)
{
  int i;
  for (i = 0; i < nfds; i++) {
    if (fds[i].fd != -1)
      return TRUE;
  }
  return FALSE;
}


static void
reap_children(int wait)
{
  int i, j;
  pid_t pid;
  int opts;

  opts = wait ? 0 : WNOHANG;

  for (i = 0; i < env.njobs; i++) {
    if (env.procs[i].pid == -1)
      continue;
    pid = waitpid(env.procs[i].pid, &env.procs[i].status, opts);
    if (pid == -1)
      error(0, errno, "waitpid failed for the process %u",
            (unsigned)env.procs[i].pid);
    else if (pid > 0) {         /* child did finish */
      if (WIFEXITED(env.procs[i].status)) {
        message(0, "child terminated: exit(%d)",
                WEXITSTATUS(env.procs[i].status));
      }
      else if (WIFSIGNALED(env.procs[i].status)) {
        message(0, "child terminated: signal(%d)",
                WTERMSIG(env.procs[i].status));
      }
      else {
        error(0, 0, "child terminated: unknown");
      }

      for (j = 0; j < 3; j++) {
        env.fds[1 + i + j * env.njobs].fd = -1;

        close(env.procs[i].fds[j]);
        env.procs[i].fds[j] = -1;
      }
    }
    else {
      /* superflous wake up? */
      error(0, 0, "warning: superfluous child signal");
    }
  }
}


static int
cleanup_server(void)
{
  int i;

  for (i = 0; i < env.njobs; i++) {
    if (env.procs[i].fds[STDIN_FILENO] != -1) {
      close(env.procs[i].fds[STDIN_FILENO]);
      env.procs[i].fds[STDIN_FILENO] = -1;
    }
  }

  reap_children(TRUE);

  return 0;
}


static int
start_server(void)
{
  int nevents;
  char buf[PIPE_BUF];

  int use_oldline = FALSE;
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;

  int i;
  int written;

  env.fds = prepare_poll(&env.nfds, env.sig_fds[P_READ], env.procs, env.njobs);

  while (TRUE) {
    if (!has_valid_fd(env.fds, env.nfds)) {
      debug(0, "all FDs are invalid, break the main loop");
      break;
    }

    nevents = ppoll(env.fds, env.nfds, NULL, &env.sigmask);
    if (nevents == -1) {
      error(1, errno, "poll(2) failed");
      /* TODO: graceful exit? */
    }
    else {
      debug(0, "ppoll say, %d event(s) found", nevents);
    }

    if (env.fds[0].revents & POLLIN) { /* one or more children dead */
      read(env.fds[0].fd, buf, PIPE_BUF);
      reap_children(FALSE);
      nevents--;
    }

    for (i = 1; i < env.nfds; i++) {
      if (nevents <= 0)
        break;

      if (env.fds[i].revents & POLLOUT) {
        nevents--;

        if (!use_oldline)
          linelen = getline(&line, &linecap, env.in);

        if (linelen <= 0)       /* ran out of input. */
          goto end;

        written = write(env.fds[i].fd, line, linelen);
        if (written == -1) {
          error(0, errno, "can't write to the child %u",
                env.procs[(i - 1) % env.njobs].pid);
          env.fds[i].fd = -1;
          use_oldline = TRUE;
        }
        else
          use_oldline = FALSE;
      }
      if (env.fds[i].revents & env.poll_errmask) {
        error(0, errno, "poll error for the child %u",
              env.procs[(i - 1) % env.njobs].pid);
        env.fds[i].fd = -1;
      }
    }
  }
 end:
  fclose(env.in);
  env.in = NULL;

  /* TODO:  */
  return 0;
}


static struct pollfd *
prepare_poll(size_t *nfds, int sigfd, struct proc *procs, size_t njobs)
{
  int i;
  struct pollfd *pfd, *p;

  pfd = malloc(sizeof(struct pollfd) * (njobs + 1));
  if (nfds)
    *nfds = njobs + 1;

  if (!pfd)
    return NULL;
  p = pfd;

  p->fd = sigfd;
  p->events = POLLIN;
  p->revents = 0;
  p++;

  for (i = 0; i < njobs; i++, p++) {
    p->fd = procs[i].fds[STDIN_FILENO];
    p->events = POLLOUT;
    p->revents = 0;
  }

#ifndef NDEBUG
  for (i = 0; i < njobs + 1; i++) {
    debug(0, "pollfd: fd(%02d) events(%04x)", pfd[i].fd, pfd[i].events);
  }
#endif  /* NDEBUG */

  return pfd;
}


static struct proc *
spawn_children(size_t nproc, int argc, char *argv[])
{
  struct proc *p;
  int i;
  int p_in[2];
  int outfd;
  char buf[PATH_MAX];

  p = malloc(sizeof(struct proc) * nproc);
  if (!p)
    return NULL;

  for (i = 0; i < nproc; i++) {
    p[i].pid = -1;
    p[i].fds[0] = p[i].fds[1] = p[i].fds[2] = -1;
  }

  for (i = 0; i < nproc; i++) {
    if (pipe(p_in) == -1)
      error(1, errno, "cannot create pipe");

#ifndef NDEBUG
    debug(0, " p_in[0] = %d", p_in[0]);
    debug(0, " p_in[1] = %d", p_in[1]);
#endif

    if (env.out_tmpl && strcmp(env.out_tmpl, "-") != 0) {
      strncpy(buf, env.out_tmpl, PATH_MAX - 1);
      buf[PATH_MAX - 1] = '\0';
      if ((outfd = mkstemp(buf)) == -1) {
        error(0, errno, "can't create a temporary file");
        close(p_in[0]);
        continue;
      }
      else
        message(0, "using \"%s\" for the STDOUT of child#%d", buf, i);
      p[i].fds[STDOUT_FILENO] = outfd;
    }

    if (env.err_tmpl && strcmp(env.err_tmpl, "-") != 0) {
      strncpy(buf, env.err_tmpl, PATH_MAX - 1);
      buf[PATH_MAX - 1] = '\0';
      if ((outfd = mkstemp(buf)) == -1) {
        error(0, errno, "can't create a temporary file");
        close(p_in[0]);
        close(p[i].fds[STDOUT_FILENO]);
        continue;
      }
      else
        message(0, "using \"%s\" for the STDERR of child#%d", buf, i);
      p[i].fds[STDERR_FILENO] = outfd;
    }

    p[i].pid = fork();

    if (p[i].pid == -1) {       /* error */
      error(0, errno, "fork(2) failed");
    }
    else if (p[i].pid == 0) {   /* child */
      close(p_in[P_WRITE]);

      dup2(p_in[P_READ], STDIN_FILENO);
      close(p_in[P_READ]);

      if (p[i].fds[STDOUT_FILENO] != -1) {
        dup2(p[i].fds[STDOUT_FILENO], STDOUT_FILENO);
        close(p[i].fds[STDOUT_FILENO]);
      }

      if (p[i].fds[STDERR_FILENO] != -1) {
        dup2(p[i].fds[STDERR_FILENO], STDERR_FILENO);
        close(p[i].fds[STDERR_FILENO]);
      }

#if 0
      for (i = 3; i < 100; i++) {
        if (close(i) != -1) {
          fprintf(stderr, "child closing fd %d\n", i);
        }
      }
#endif  /* 0 */

      child_main(argc, argv);
    }
    else {                      /* parent */
      close(p_in[P_READ]);

      if (p[i].fds[STDOUT_FILENO] != -1) {
        /* TODO: I don't know if we need to keep the fd here. */
        close(p[i].fds[STDOUT_FILENO]);
      }
      if (p[i].fds[STDERR_FILENO] != -1) {
        /* TODO: I don't know if we need to keep the fd here. */
        close(p[i].fds[STDERR_FILENO]);
      }

      p[i].fds[STDIN_FILENO] = p_in[P_WRITE];
    }
  }
  return p;
}


static void
child_main(int argc, char *argv[])
{
#if 1
  int ret;
  ret = execvp(argv[0], argv);
  if (ret == -1)
    error(1, errno, "execvp(2) failed");
#else
  char buf[LINE_MAX];
  while (fgets(buf, LINE_MAX, stdin)) {
    printf("child(%u): %s", (unsigned)getpid(), buf);
  }
  exit(0);
#endif  /* 0 */
}


static void
handler_sigchild(int signo)
{
  static char buf[1] = "!";
  int ret;

  //write(STDERR_FILENO, "SIGNAL\n", 7);
  ret = write(env.sig_fds[P_WRITE], buf, 1);

  //if (ret == -1)
  //  abort();
}


static void
prepare_signals(void)
{
  int ret;
  struct sigaction act;

  ret = pipe(env.sig_fds);
  if (ret == -1)
    error(1, errno, "pipe(2) failed");

  set_closexec(env.sig_fds[P_WRITE]);
  set_closexec(env.sig_fds[P_READ]);

  set_nonblock(env.sig_fds[P_WRITE]);
  set_nonblock(env.sig_fds[P_READ]);

  memset(&act, 0, sizeof(act));

  act.sa_handler = handler_sigchild;
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask);

  if (sigaction(SIGCHLD, &act, NULL) == -1)
    error(1, errno, "cannot register SIGCHLD handler");

  sigemptyset(&env.sigmask);
  sigemptyset(&env.poll_sigmask);

  sigprocmask(0, NULL, &env.poll_sigmask);

  {
    sigset_t blk;
    sigemptyset(&blk);
    sigaddset(&blk, SIGCHLD);
    sigprocmask(SIG_BLOCK, &blk, NULL);
    sigprocmask(0, NULL, &env.sigmask);
  }
  /* SIGCHLD is blocked from now on.  To enable it, use env.poll_sigmask
   * temporarily. */
}


int
set_closexec(int fd)
{
  int flags = fcntl(fd, F_GETFD);

  if (flags == -1) {
    fprintf(stderr, "error: fcntl(fd, F_GETFD) failed: %s",
            strerror(errno));
    return -1;
  }
  if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
    fprintf(stderr, "error: fcntl(fd, F_SETFD) failed: %s",
            strerror(errno));
    return -1;
  }
  return 0;
}


int
set_nonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL);

  if (flags == -1) {
    fprintf(stderr, "error: fcntl(fd, F_GETFL) failed: %s",
            strerror(errno));
    return -1;
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    fprintf(stderr, "error: fcntl(fd, F_SETFL) failed: %s",
            strerror(errno));
    return -1;
  }
  return 0;
}


#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/param.h>
#include <sys/sysctl.h>
#else
#include <unistd.h>
#endif

int
getncores(void)
{
#ifdef WIN32
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  return sysinfo.dwNumberOfProcessors;
#elif defined(__APPLE__)
  int nm[2];
  size_t len = 4;
  uint32_t count;

  nm[0] = CTL_HW; nm[1] = HW_AVAILCPU;
  sysctl(nm, 2, &count, &len, NULL, 0);

  if(count < 1) {
    nm[1] = HW_NCPU;
    sysctl(nm, 2, &count, &len, NULL, 0);
    if(count < 1) { count = 1; }
  }
  return count;
#else
  return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}


void
debug(int ecode, const char *fmt, ...)
{
  va_list ap;

  if (!env.debug)
    return;

  va_start(ap, fmt);
  msgv_out(0, ecode, fmt, ap);
  va_end(ap);
}


void
message(int ecode, const char *fmt, ...)
{
  va_list ap;

  if (!env.verbose)
    return;

  va_start(ap, fmt);
  msgv_out(0, ecode, fmt, ap);
  va_end(ap);
}


static void
msgv_out(int status, int ecode, const char *fmt, va_list ap)
{
  fflush(stdout);
  flockfile(stderr);

  fprintf(stderr, "%s: ", program_name);

  if (ecode)
    fprintf(stderr, "%s: ", strerror(ecode));

  vfprintf(stderr, fmt, ap);

  fputc('\n', stderr);

  fflush(stderr);
  funlockfile(stderr);

  if (status)
    exit(status);
}


#ifdef __APPLE__
#if 0
void
error(int status, int ecode, const char *fmt, ...)
{
  va_list ap;

  fflush(stdout);

  flockfile(stderr);

  fprintf(stderr, "%s: ", program_name);

  if (ecode)
    fprintf(stderr, "%s: ", strerror(ecode));

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  fputc('\n', stderr);
  funlockfile(stderr);

  if (status)
    exit(status);
}
#endif  /* 0 */
#endif  /* MACOS */


static void
version_and_exit(void)
{
  printf("distribute version %s\n", VERSION_STRING);
  exit(0);
}


static void
usage(void)
{
  static const char *msgs[] = {
    "distribute input lines to multiple processes",
    "usage: XXX [OPTION...] COMMAND [ARG...]",
    "",
    "  -j XX,   --jobs=XX           set number of workers to XX (processes)",
    "  -i FILE, --input=FILE        input is from FILE",
    "",
    "  -O OUT, --output=OUT         child STDOUT will be stored in OUTxxxxxx",
    "  -E ERR, --output=ERR         child STDERR will be stored in ERRxxxxxx",
    "",
    "  -v      --verbose            set verbose mode",
    "",
    "           --help              show help message and exit",
    "           --version           show version and exit",
    "",
    "Report bugs to cinsky@gmail.com",
  };
  int i;

  for (i = 0; i < sizeof(msgs) / sizeof(const char *); i++)
    puts(msgs[i]);

  exit(0);
}
