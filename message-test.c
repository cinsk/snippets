#include "message.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

#define THREAD_MAX      10


struct msg_category *mc_mux;
struct msg_category *mc_sink;
struct msg_category *mc_stdout;
struct msg_category *mc_file;


static void
block_sighup()
{
  sigset_t set;
  int ret;
  sigemptyset(&set);
  sigaddset(&set, SIGHUP);
  ret = pthread_sigmask(SIG_BLOCK, &set, NULL);
  if (ret)
    fprintf(stderr, "pthread_sigmask failed: %s\n", strerror(ret));
}


void test_multiplex(void)
{
  c_warn(mc_sink, "hello there!");
}


int ready_threads = 0;


void *
producer(void *arg)
{
  long i = 0;
  struct msg_category *mc;
  struct timeval tv;
  long long begin, end;

  block_sighup();

  __sync_add_and_fetch(&ready_threads, 1);

  while (ready_threads < THREAD_MAX)
    sched_yield();

  m_info("Go!");

  mc = message_get_category(arg);

  gettimeofday(&tv, NULL);
  begin = tv.tv_sec * 1000000 + tv.tv_usec;

  while (1) {
    i++;
    if (i % 100 == 0) {
      gettimeofday(&tv, NULL);
      end = tv.tv_sec * 1000000 + tv.tv_usec;

      if (end - begin > 5000000)
        break;
    }
    message_c(mc, (i % 5 + 1) * 100, "producing %ld", i);
#if 0
    //sleep(1);
    if (i % 100 == 0)
      sched_yield();
#endif  /* 0 */
  }
  message_unget_category(mc);
  m_info("END: %ld", i);

  pthread_exit(NULL);
  return NULL;
}


void
benchmark(struct msg_category *mc)
{
  struct timeval tv;
  long long begin, end;
  int i;

  gettimeofday(&tv, NULL);
  begin = tv.tv_sec * 1000000 + tv.tv_usec;

  for (i = 0; i < 10000; i++) {
    message_c(mc, (i % 5 + 1) * 100, "producing %d", i);
#if 0
    //sleep(1);
    if (i % 100 == 0)
      sched_yield();
#endif  /* 0 */
  }
  gettimeofday(&tv, NULL);
  end = tv.tv_sec * 1000000 + tv.tv_usec;

  message_c(mc, MCL_FATAL, "TIME: %lld", end - begin);
}


void
test_multithreads(const char *name)
{
  pthread_t threads[THREAD_MAX];
  int i;

  ready_threads = 0;
  //block_sighup();

  for (i = 0; i < THREAD_MAX; i++)
    pthread_create(&threads[i], NULL, producer, (void *)name);

  for (i = 0; i < THREAD_MAX; i++)
    pthread_join(threads[i], NULL);
}


// kill -HUP `ps -ef | awk '$8 ~ /a\.out/ { print $2 }'`
int
main(int argc, char *argv[])
{
  if (argc > 1)
    message_init(argv[1], NULL);
  else
    message_init(NULL, NULL);

  message_thread_init(NULL);

  //mc_file = message_get_category("log");

#if 0
  mc_stdout = message_file_category("stdout", "/dev/stdout");
  mc_file = message_file_category("file", "msg.out");
  mc_sink = message_multiplex_category("multiplex",
                                       mc_stdout, mc_file, MC_DEFAULT, NULL);
  mc_sink->fmt_prologue = strdup("[%P:%h] (%t) ");
  mc_sink->fmt_time = strdup("%FT%T");

  m_info("hello");
  test_multiplex();
#endif  /* 0 */

  mc_mux = message_get_category("multi");

#if 1
  m_info("before multithreads..");
  test_multithreads("log");
  m_info("after multithreads..");
#endif  /* 0 */

  //benchmark(mc_file);
  message_unget_category(mc_mux);

  return 0;
}
