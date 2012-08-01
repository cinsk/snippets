#include "message.h"
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define THREAD_MAX      1


struct msg_category *mc_mux;
struct msg_category *mc_sink;
struct msg_category *mc_stdout;
struct msg_category *mc_file;

void test_multiplex(void)
{
  c_warn(mc_sink, "hello there!");
}


int ready_threads = 0;

void *
producer(void *arg)
{
  int i;
  struct msg_category *mc = (struct msg_category *)arg;

  __sync_add_and_fetch(&ready_threads, 1);

  while (ready_threads < THREAD_MAX)
    sched_yield();

  for (i = 0; i < 1000; i++) {
    message_c(mc, (i % 5 + 1) * 100, "producing %d", i);
#if 0
    //sleep(1);
    if (i % 100 == 0)
      sched_yield();
#endif  /* 0 */
  }

  return NULL;
}


void
test_multithreads(struct msg_category *mc)
{
  pthread_t threads[THREAD_MAX];
  int i;

  ready_threads = 0;
  for (i = 0; i < THREAD_MAX; i++)
    pthread_create(&threads[i], NULL, producer, mc);

  for (i = 0; i < THREAD_MAX; i++)
    pthread_join(threads[i], NULL);
}


int
main(int argc, char *argv[])
{
  message_init(argv[1]);
  message_thread_init(NULL);
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

  m_info("before multithreads..");
  test_multithreads(mc_mux);
  m_info("after multithreads..");

  return 0;
}
