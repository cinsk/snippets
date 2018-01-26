/*
 * Build:
 *
 * - On SmartOS:  gcc -pthread condwait.c -lm -lpthread
 * - On LX/Linux: gcc -pthread -DUSE_MONOTONIC -DCOND_SETCLOCK condwait.c -lm -lpthread
 */
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#ifdef USE_MONOTONIC
# define CLOCK_ID CLOCK_MONOTONIC
#else
# define CLOCK_ID CLOCK_REALTIME
#endif

#define NUM_ITERATION   50

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void
error(int status, int err, const char *fmt, ...)
{
  va_list ap;
  char *msg = NULL;

  fflush(stdout);

  va_start(ap, fmt);
  vasprintf(&msg, fmt, ap);
  va_end(ap);

  if (err != 0) {
    fprintf(stderr, "error: %s: %s\n", msg, strerror(err));
  }
  else {
    fprintf(stderr, "error: %s\n", msg);
  }
  fflush(stderr);

  free(msg);
  if (status) {
    exit(status);
  }
}


void
timespec_normalize(struct timespec *ts)
{                               /* cannot handle all cases such as negative tv_nsec. */
  if (ts->tv_nsec >= 1000000000) {
    int nsec = ts->tv_nsec / 1000000000;
    ts->tv_nsec = ts->tv_nsec % 1000000000;
    ts->tv_sec += nsec;
  }
}

void
timespec_add(struct timespec *x, const struct timespec *y)
{
  x->tv_sec += y->tv_sec;
  x->tv_nsec += y->tv_nsec;
  timespec_normalize(x);
}

void
timespec_subtract(struct timespec *result, const struct timespec *x, const struct timespec *y)
{                               /* stole from glibc's manual */
  result->tv_sec = y->tv_sec - x->tv_sec;
  result->tv_nsec = y->tv_nsec - x->tv_nsec;

  if (y->tv_nsec < x->tv_nsec) {
    result->tv_nsec = y->tv_nsec + 1000000000 - x->tv_nsec;
    result->tv_sec = y->tv_sec - x->tv_sec - 1;
  }
  else {
    result->tv_nsec = y->tv_nsec - x->tv_nsec;
    result->tv_sec = y->tv_sec - x->tv_sec;
  }
}

long double timespec2double(const struct timespec *ts)
{
  long double d = ts->tv_nsec;
  d /= 1000000000.0;
  d += ts->tv_sec;
  return d;
}

/*
 * Return the list of integers, Y (min <= Y <= max) using Y = a * X^degree, where 0 <= X < size.
 */
long *
polynomial_dist(int size, int degree, long min, long max)
{
  int i;
  long *result = malloc(sizeof(long) * size);
  if (result == NULL)
    return NULL;

  double a = (max - min) / pow(size, degree);

  for (i = 0; i < size; i++) {
    result[i] = (long)(pow(i, degree) * a + min);
  }

  return result;
}

int
main(void)
{
  int err, conderr;
  long *input_set;
  int i, size = NUM_ITERATION;

  input_set = polynomial_dist(size, 4, 0, 10000000);

  pthread_condattr_t condattr;
  pthread_condattr_init(&condattr);
#ifdef COND_SETCLOCK
  pthread_condattr_setclock(&condattr, CLOCK_ID);
#endif
  pthread_cond_init(&cond, &condattr);

  for (i = 0; i < size; i++) {
    struct timespec ts_begin, ts_deadline, ts_end, ts_diff;

    err = pthread_mutex_lock(&mutex);
    if (err != 0)
      error(1, err, "pthread_mutex_lock() failed");

    err = clock_gettime(CLOCK_ID, &ts_begin);
    if (err != 0)
      error(1, errno, "clock_gettime() failed");

    ts_deadline = ts_begin;
    ts_deadline.tv_nsec += input_set[i];
    timespec_normalize(&ts_deadline);

    conderr = pthread_cond_timedwait(&cond, &mutex, &ts_deadline);

    err = clock_gettime(CLOCK_ID, &ts_end);
    if (err != 0)
      error(1, errno, "clock_gettime() failed");

    timespec_subtract(&ts_diff, &ts_deadline, &ts_end);

    if (conderr != 0) {
      if (conderr == ETIMEDOUT) {
        printf("TIMEOUT,%.9Lf,%ld.%09ld\n",
               (long double)input_set[i] / 1000000000, ts_diff.tv_sec, ts_diff.tv_nsec);
      }
      else
        error(1, conderr, "pthread_cond_timedwait() failed");

      err = pthread_mutex_unlock(&mutex);
      if (err != 0)
        error(1, err, "pthread_mutex_unlock() failed");
    }
    else {                      /* unlikely, except suprious wakeup */
      printf("SUCCESS %Lf %ld.%09ld\n",
             (long double)input_set[i] / 1000000000, ts_diff.tv_sec, ts_diff.tv_nsec);
    }
  }

  return 0;
}
