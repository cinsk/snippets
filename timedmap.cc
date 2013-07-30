#include "timedmap.hh"

class Foo {
  const char *prefix_;

public:
  Foo(const Foo &f) : prefix_(f.prefix_) {
    xdebug(0, "Foo(%s) Copy CTOR", prefix_);
  }
  Foo(const char *prefix = "") : prefix_(prefix)  {
    xdebug(0, "Foo(%s) CTOR", prefix_);
  }
  ~Foo() { xdebug(0, "Foo(%s) DTOR", prefix_); }

  const char *what() const { return "Foo"; }
};


TMAP_TYPE_DECL(mymap_t, int, Foo);
// typedef timedmap<int, Foo> mymap_t;
// typedef timedmap<int, Foo>::timedmap_getter mymap_tgetter;

#define CHILDREN_MAX       5

pthread_t children[CHILDREN_MAX];
int child_id = 0;

static void *child_main(void *arg);

int
main(void)
{
  xerror_init(0, 0);
  xthread_set_name("main");

  mymap_t tmap(2);

#if 1
  tmap.set(99, Foo("timedmap"));

  bool cond = true;

  while (cond) {
    TMAP_GET(mymap_t, tmap, 99, g) {
      if (g)
        xerror(0, 0, "what: %s", g->what());
      else {
        xerror(0, 0, "expired");
        cond = false;
      }
      //g.erase();
    }
    usleep(500000);
  }

#endif  // 0
  tmap.exist(10);
  tmap.erase(10);
  for (size_t i = 0; i < sizeof(children) / sizeof(pthread_t); i++) {
    pthread_create(children + i, 0, child_main, (void *)&tmap);
  }

  for (size_t i = 0; i < sizeof(children) / sizeof(pthread_t); i++) {
    pthread_join(children[i], 0);
  }


  xerror(0, 0, "map size: %d", (int)tmap.size());
  return 0;
}


static void *
child_main(void *arg)
{
  mymap_t *map = (mymap_t *)arg;

  int id = __sync_add_and_fetch(&child_id, 1);

  xthread_set_name("child#%d", id);

  map->set(id, Foo("child"));

  bool cond = true;

  while (cond) {
    TMAP_GET(mymap_t, *map, id, g) {
      if (!g) {
        cond = false;
        break;
      }
      xerror(0, 0, "child#%d: %s", id, g->what());
      usleep((rand() % 30) * 100000);
    }

    usleep((rand() % 10) * 100000);
  }

  return 0;
}
