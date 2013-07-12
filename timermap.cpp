#include <boost/shared_ptr.hpp>
#include <time.h>

#include "timermap.h"


class Foo {
public:
  Foo() {
    xdebug(0, "Foo::CTOR");
  }
  ~Foo() {
    xdebug(0, "Foo::DTOR");
  }

  const char *what() {
    return "foo";
  }

};


int
main(void)
{
  xerror_init(0, 0);

  xerror(0, 0, "pid: %d", getpid());

  typedef TimerMap<int, boost::shared_ptr<Foo> > TMAP;
  TMAP m(2);
  //int i = 4;

  {
    boost::shared_ptr<Foo> ptr(new Foo());
    m.set(0, ptr);
  }
  {
    boost::shared_ptr<Foo> ptr(new Foo());
    m.set(1, ptr);
  }

  bool loop = true;

  loop = true;
  while (loop) {
#if 0
    try {
      TMAP::Ptr ptr = m.get(1);
      xerror(0, 0, "main: 1 = %s", (*ptr)->what());
    }
    catch (std::out_of_range &e) {
      xerror(0, 0, "main: exception: %s", e.what());
      loop = false;
    }
#else
    TMAP::Ptr ptr = m.get(1);
    if (ptr)
      xerror(0, 0, "main: 1 = %s", (*ptr)->what());
    else {
      xerror(0, 0, "main: not found");
      //loop = false;
    }

#endif  // 0
    usleep(100000);
  }

  //printf("%p\n", (void *)TimerMap<int, boost::shared_ptr<int> >::timer_main);
  return 0;
}
