#include <stdio.h>
#include <stdlib.h>
#include "heap.h"

struct node {
  int data;
  char dummy[20];
};

#define nodeheap_t_cmp(x, y)    ((x)->data - (y)->data)

int
main(int argc, char *argv[])
{
  struct node *p;
  HEAP_DECL_TYPE(struct node, nodeheap_t);
  int i;

  int maxsize = atoi(argv[1]);

  nodeheap_t heap = HEAP_INITIALIZER(maxsize);

  fprintf(stderr, "sizeof(node) = %zd\n", sizeof(*p));

  for (i = 0; i < maxsize; ++i) {
    p = malloc(sizeof(*p));
    if (!p)
      abort();
    p->data = rand() % 1000;
    printf("%p - %d (%zu)\n", p, p->data, sizeof(*p));
    HEAP_INSERT(nodeheap_t, &heap, p);
    __builtin___clear_cache(p, ((char *)p) + sizeof(*p));
  }

  printf("\n\n--\n");
  while (1) {
    p = HEAP_POP(nodeheap_t, &heap);

    if (!p)
      break;


    printf("%p - %d\n", p, p->data);
    free(p);
  }

  HEAP_FREE(&heap);

  return 0;
}
