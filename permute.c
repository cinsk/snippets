#include <stdlib.h>
#include <setjmp.h>
#include "permute.h"

#define USE_SETJMP

typedef int (*callback)(const int *elem, size_t nelem, void *data);

struct arg {
  int n;
  int k;
  int count;
#ifdef USE_SETJMP
  jmp_buf env;
#endif
  callback cb;
  void *data;

  int set[0];
};

#ifdef USE_SETJMP
#define CALL_CB(ap, nelem)     do {                     \
    int ret = 0;                                        \
    if ((ap)->cb)                                       \
      ret = (ap)->cb((ap)->set, (nelem), (ap)->data);   \
    if (ret)                                            \
      longjmp(ap->env, ret);                            \
  } while (0)
#else
#define CALL_CB(ap)     do {                            \
    if ((ap)->cb)                                       \
      (ap)->cb((ap)->set, (nelem), (ap)->data);         \
  } while (0)
#endif

static void permute_(struct arg *ap, int start);
static void ksubset_(struct arg *ap, int start, int setidx);
static void subset_(struct arg *ap, int start, int setidx);


static __inline__ struct arg *
new_arg(int n, callback cb, void *data)
{
  struct arg *ap;
  int i;

  ap = malloc(sizeof(*ap) + sizeof(int) * n);
  for (i = 0; i < n; i++)
    ap->set[i] = i;

  ap->count = 0;
  ap->n = ap->k = 0;
  ap->cb = cb;
  ap->data = data;

  return ap;
}


int
permute(size_t n, callback cb, void *data)
{
  struct arg *ap;

  ap = new_arg(n, cb, data);
  ap->n = n;

#ifdef USE_SETJMP
  if (setjmp(ap->env) == 0)
#endif
    permute_(ap, 0);

  free(ap);

  return ap->count;
}


static void
permute_(struct arg *ap, int start)
{
  int i;
  int t;

  if (start >= ap->n) {
    ap->count++;
    CALL_CB(ap, ap->n);
    return;
  }

  for (i = start; i < ap->n; i++) {
    t = ap->set[start];
    ap->set[start] = ap->set[i];
    ap->set[i] = t;

    permute_(ap, start + 1);

    t = ap->set[start];
    ap->set[start] = ap->set[i];
    ap->set[i] = t;
  }
}


int
ksubset(int n, int k, callback cb, void *data)
{
  struct arg *ap = new_arg(k, cb, data);

  ap->n = n;
  ap->k = k;

#ifdef USE_SETJMP
  if (setjmp(ap->env) == 0)
#endif
    ksubset_(ap, 0, 0);

  free(ap);
  return ap->count;
}


static void
ksubset_(struct arg *ap, int start, int setidx)
{
  int i;

  if (setidx >= ap->k) {
    ap->count++;
    CALL_CB(ap, setidx);
    return;
  }

  for (i = start; i <= (ap->n - ap->k + setidx); i++) {
    ap->set[setidx] = i;
    ksubset_(ap, i + 1, setidx + 1);
  }
}


int
subset(int n, callback cb, void *data)
{
  struct arg *ap = new_arg(n, cb, data);
  ap->n = n;

#ifdef USE_SETJMP
  if (setjmp(ap->env) == 0)
#endif
    subset_(ap, 0, 0);

  free(ap);
  return ap->count;
}


static void
subset_(struct arg *ap, int start, int setidx)
{
  if (start >= ap->n) {
    ap->count++;
    CALL_CB(ap, setidx);
    return;
  }

  ap->set[setidx] = start;
  subset_(ap, start + 1, setidx + 1);
  subset_(ap, start + 1, setidx);
}





#ifdef TEST_PERMUTE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>

const char *program_name;

int
help_and_exit(int argc, char *argv[])
{
  static const char *msgs[] = {
    "",
    "  permute N     permute all case(s) of the N-set",
    "  subset N      find all subset of the N-set",
    "  ksubset N K   find all K-elements subset of the N-set",
    "",
    "  help          help and exit",
    "",
    "N-set is the set of the integer from zero to N - 1, { 0, ..., N - 1 }",
    "",
  };
  int i;

  printf("Usage: %s COMMAND [ARGUMENT...]\n", program_name);
  for (i = 0; i < sizeof(msgs) / sizeof(const char *); i++)
    puts(msgs[i]);

  exit(0);
  return 0;
}

static int
mycb(const int *elem, size_t nelem, void *data)
{
  int i;
  for (i = 0; i < nelem; i++)
    printf("%d ", elem[i]);
  putchar('\n');

  if (nelem == 3 && elem[0] == 1 && elem[1] == 2 && elem[2] == 3)
    return 1;

  return 0;
}


static int
test_permute(int argc, char *argv[])
{
  int total;
  int n;

  if (argc != 1) {
    fprintf(stderr, "error: wrong number of argument(s)\n");
    return -1;
  }

  n = atoi(argv[0]);
  total = permute(n, mycb, 0);
  printf("# Total %d case(s)\n", total);
  return 0;
}


static int
test_ksubset(int argc, char *argv[])
{
  int total;
  int n, k;

  if (argc != 2) {
    fprintf(stderr, "error: wrong number of argument(s)\n");
    return -1;
  }

  n = atoi(argv[0]);
  k = atoi(argv[1]);
  total = ksubset(n, k, mycb, 0);
  printf("# Total %d case(s)\n", total);
  return 0;
}


static int
test_subset(int argc, char *argv[])
{
  int total;
  int n;

  if (argc != 1) {
    fprintf(stderr, "error: wrong number of argument(s)\n");
    return -1;
  }

  n = atoi(argv[0]);
  total = subset(n, mycb, 0);
  printf("# Total %d case(s)\n", total);
  return 0;
}


struct command {
  const char *name;
  int (*proc)(int, char *[]);
} command_map[] = {
  { "permute", test_permute },
  { "subset",  test_subset },
  { "ksubset", test_ksubset },
  { "help", help_and_exit },
  { 0, 0 },
};


int
main(int argc, char *argv[])
{
  const char *command;
  int i;

  program_name = basename(argv[0]);
  argc--; argv++;

  if (argc == 1)
    help_and_exit(0, 0);

  command = argv[0];
  argc--; argv++;

  for (i = 0; command_map[i].name != 0; i++) {
    if (strcmp(command, command_map[i].name) == 0) {
      command_map[i].proc(argc, argv);
      break;
    }
  }

  return 0;
}
#endif  /* TEST_PERMUTE */
