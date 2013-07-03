#include "rbtree.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "difftime.h"

struct mytype {
  struct rb_node node_;
  char *name;
  int value;
};


struct mytype *
my_search(struct rb_root *root, char *string)
{
  return RB_SEARCH(root, struct mytype, node_, name, strcmp, string);
}


int
my_insert(struct rb_root *root, const char *name, int value)
{
  struct mytype *p;
  struct mytype *ret;

  p = malloc(sizeof(*p));
  p->name = strdup(name);
  p->value = value;

  ret = RB_INSERT(root, struct mytype, name, node_, strcmp, p);

  if (ret) {                      /* key already exists */
    return -1;
  }
  return 0;
}


void
my_delete(struct rb_root *root, const char *name)
{
  struct mytype *p;

  p = RB_DELETE(root, struct mytype, node_, name, strcmp, name);
  free(p->name);
  free(p);
}


void
my_clear1(struct rb_root *root)
{
#if 0
  while (!RB_EMPTY_ROOT(root)) {
    struct mytype *p = container_of(root->rb_node, struct mytype, node_);
    rb_erase(root->rb_node, root);
    free(p->name);
    free(p);
  }
#endif

  RB_ERASE_LOOP(root) {
    struct mytype *p = RB_ERASE(root, struct mytype, node_);
    free(p->name);
    free(p);
  }
#if 0
  struct mytype *p;

  // for (i = 0; i < 3; i++)
  for ((p = container_of(root->rb_node, struct mytype, node_)),
        rb_erase(root->rb_node, root);
       !RB_EMPTY_ROOT(root);
       (p = container_of(root->rb_node, struct mytype, node_)),
        rb_erase(root->rb_node, root)
       ) {
    free(p->name);
    free(p);
  }
#endif
#if 0
  RB_CLEAR(root, struct mytype, node_, p) {
    free(p->name);
    free(p);
  }
#endif
}

void
my_clear2(struct rb_root *root)
{
  while (!RB_EMPTY_ROOT(root)) {
    struct mytype *p = container_of(rb_first(root),
                                    struct mytype, node_);
    rb_erase(&p->node_, root);
    free(p->name);
    free(p);
  }
}


int
main(void)
{
  //struct rb_root myroot = RB_ROOT;
  struct rb_root root = RB_ROOT;
  struct mytype *p;
  char *line = 0;
  size_t linecap;
  int readch;

  while ((readch = getline(&line, &linecap, stdin)) != -1) {
    if (line[readch - 1] == '\n')
      line[readch - 1] = '\0';

    //printf("# %s\n", line);
    p = my_search(&root, line);
    if (p)
      p->value++;
    else
      my_insert(&root, line, 1);
  }
  free(line);

  printf("size: %zd\n", root.size);

  {
    struct rb_node *ptr;
    for (ptr = rb_first(&root); ptr != 0; ptr = rb_next(ptr)) {
      p = container_of(ptr, struct mytype, node_);
      //printf("%s:%d\n", p->name, p->value);
    }
  }

  //my_delete(&root, "cinsk");
  //my_delete(&root, "tralyon");
  {
    long l = 3;

    DIFFCLOCK(l) {
      my_clear1(&root);
    }

    printf("%lf\n", ((double)l) / CLOCKS_PER_SEC);
  }
  return 0;
}
