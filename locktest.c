#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <libgen.h>
#include <getopt.h>

#if defined(__linux__)
#include <sys/syscall.h>
#endif

/*
 * build:
 *    $ gcc locktest.c -lpthread
 *
 * Usage:
 *    $ ./a.out -c 100 -t 10      # Run for 10 seconds with 100 threads for lock/increase the counter.
 *    $ ./a.out -c 100 -l 100000  # Run until the counter reaches 100000 with 100 threads.
 *    $ ./a.out                   # Run forever.
 */

uint64_t get_thread_id();
void *worker(void *arg);
void *xmalloc(size_t size);
void error(int estatus, int errnum, const char *fmt, ...);
void error_init(void);
int redirect_stderr_null();
void diff_time(struct timespec *result, struct timespec *lhs,  struct timespec *rhs);

static void show_help_and_exit(void);
static int do_main(int argc, char *argv[]);

pthread_mutex_t ready_mutex = PTHREAD_MUTEX_INITIALIZER;
int ready_num = 0;
pthread_cond_t ready_cond = PTHREAD_COND_INITIALIZER;

int num_threads = 10;

uint64_t lock_limit = 0;

uint64_t counter = 0;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

int timer_expired = 0;
int timer_duration = 0;

FILE *error_stream;

int verbose_mode = 0;
const char *program_name;


int
main(int argc, char *argv[])
{
  int opt;

  program_name = basename(argv[0]);

  error_init();

  while ((opt = getopt(argc, argv, "c:t:l:qh")) != -1) {
    switch (opt) {
    case 'c':
      num_threads = atoi(optarg);
      break;
    case 't':
      timer_duration = atoi(optarg);
      break;
    case 'l':
      {
        char *endp = 0;
        lock_limit = strtoul(optarg, &endp, 0);
        if (*endp != '\0')
          error(1, 0, "cannot convert to numeric value: %d", optarg);
      }
      break;
    case 'q':
      redirect_stderr_null();
      break;
    case 'h':
      show_help_and_exit();
      break;
    default:
      break;
    }
  }
  argc -= optind;
  argv += optind;

  return do_main(argc, argv);
}


void
diff_time(struct timespec *result, struct timespec *lhs,  struct timespec *rhs)
{
  if (rhs->tv_nsec < lhs->tv_nsec) {
    rhs->tv_nsec += 1000000000;
    rhs->tv_sec--;
  }

  result->tv_nsec = rhs->tv_nsec - lhs->tv_nsec;
  result->tv_sec = rhs->tv_sec - lhs->tv_sec;
}


void *
worker(void *arg)
{
  int ret;
  int incr = 1;
  // int i;

  uint64_t id = get_thread_id();
  //struct timespec diff, start, end;

  pthread_mutex_lock(&ready_mutex);
  while (ready_num < num_threads) {
    ready_num += incr;
    incr = 0;

    ret = pthread_cond_wait(&ready_cond, &ready_mutex);
    if (ret != 0)
      error(1, ret, "pthread_cond_wait() failed");
    // printf("%" PRIu64 ": ready_num = %d, num_threads = %d\n", id, ready_num, num_threads);
  }
  pthread_mutex_unlock(&ready_mutex);

  if (verbose_mode)
    fprintf(stderr, "%" PRIu64 ": worker started\n", id);

  while (1) {
    pthread_mutex_lock(&counter_mutex);
    counter++;

    if (timer_expired || (lock_limit != 0 && counter >= lock_limit)) {
      pthread_mutex_unlock(&counter_mutex);
      break;
    }
    pthread_mutex_unlock(&counter_mutex);
  }

  // printf("%" PRIu64 " %lld.%09ld\n", id, (long long)diff.tv_sec, diff.tv_nsec);
  // printf("%" PRIu64 ": worker ended\n", id);
  return NULL;
}


void
error_init()
{
  if (error_stream) {
    fclose(error_stream);
  }
  error_stream = fdopen(dup(fileno(stderr)), "a");
}


int
redirect_stderr_null()
{
  FILE *fp;
  int nullfd, ret;
  int errfd = fileno(stderr);

  nullfd = open("/dev/null", O_APPEND | O_WRONLY);

  if (nullfd == -1) {
    error(1, errno, "cannot open /dev/null");
  }

  fflush(stderr);
  fclose(stderr);

  ret = dup2(nullfd, errfd);
  if (ret == -1)
    error(1, errno, "dup2() failed");

  fp = fdopen(errfd, "a");
  if (fp == NULL) {
    error(1, errno, "cannot create FILE ᅥstream from the fd, %d", errfd);
  }

  stderr = fp;
  return 0;
}


void
error(int estatus, int errnum, const char *fmt, ...)
{
  va_list ap;
  char *msg = 0;
  int ret;

  fflush(stdout);
  va_start(ap, fmt);
  ret = vasprintf(&msg, fmt, ap);
  va_end(ap);

  if (ret == -1) {
    perror("vasprintf(3) failed\n");
    exit(estatus);
  }

  if (errnum != 0)
    fprintf(error_stream, "error: %s: %s\n", msg, strerror(errnum));
  else
    fprintf(error_stream, "error: %s\n", msg);

  free(msg);
  fflush(error_stream);

  if (estatus)
    exit(estatus);
}


void *
xmalloc(size_t size)
{
  void *p;
  if (size == 0)
    size = 1;

  p = malloc(size);
  if (!p)
    error(1, errno, "malloc(3) failed");
  return p;
}


uint64_t
get_thread_id()
{
#if defined(__sun__)
  return (int64_t)pthread_self();
#elif defined(__linux__)
  return syscall(SYS_gettid);
#elif defined(__MACH__)
  uint64_t id;
  int ret;
  if ((ret = pthread_threadid_np(NULL, &id)) != 0)
    error(1, ret, "pthread_threadid_np() failed");
  return id;
#else
# error no implementation
#endif
}


static void
show_help_and_exit(void)
{
  static const char *msg[] = {
    "",
    "  -c N     set the concurrency level (# of threads) to N",
    "  -t T     run for T second(s)",
    "  -l L     stop if the counter reached to L",
    "",
    "  -h       show help messages and exit",
    "",
    "  This program will create N threads, and each thread will try to get a lock,",
    "Increase the global counter value by one, and if the counter value already reached",
    "to L (if -l option provided) it terminates,  Otherwise it will run for T seconds.",
    "After all threads terminated, it will print the number of seconds it spent.",
    "",
  };
  int i;

  printf("Usage: %s [OPTION...]\n", program_name);
  for (i = 0; i < sizeof(msg) / sizeof(msg[0]); i++) {
    puts(msg[i]);
  }

  exit(0);
}


static int
do_main(int argc, char *argv[])
{
  int i, ret;
  struct timespec ts_begin, ts_end, ts_diff;
  pthread_t *threads;

  threads = xmalloc(sizeof(pthread_t) * num_threads);

  for (i = 0; i < num_threads; i++) {
    ret = pthread_create(threads + i, NULL, worker, NULL);
    if (ret != 0)
      error(1, ret, "pthread_create() failed");
  }

  while (1) {
    pthread_mutex_lock(&ready_mutex);
    if (ready_num >= num_threads) {
      pthread_mutex_unlock(&ready_mutex);
      break;
    }
    pthread_mutex_unlock(&ready_mutex);
  }

  fprintf(stderr, "%d worker threads are ready\n", ready_num);
  pthread_mutex_lock(&ready_mutex);
  fprintf(stderr, "start\n");

  clock_gettime(CLOCK_MONOTONIC, &ts_begin);

  pthread_cond_broadcast(&ready_cond);
  pthread_mutex_unlock(&ready_mutex);

  if (timer_duration > 0) {
    sleep(timer_duration);

    pthread_mutex_lock(&counter_mutex);
    timer_expired = 1;
    pthread_mutex_unlock(&counter_mutex);
  }

  for (i = 0; i < num_threads; i++) {
    pthread_join(threads[i], NULL);
  }

  clock_gettime(CLOCK_MONOTONIC, &ts_end);

  diff_time(&ts_diff, &ts_begin, &ts_end);
  printf("time spent: %ld.%09ld\n", (long)ts_diff.tv_sec, (long)ts_diff.tv_nsec);

  pthread_mutex_lock(&counter_mutex);
  printf("counter: %" PRIu64 "\n", counter);
  pthread_mutex_unlock(&counter_mutex);

  return 0;
}
