#include <boost/shared_ptr.hpp>
#include <time.h>

#include "timermap.h"


time_t
gettime()
{
  return time(0);
}



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

  typedef TimerMap<int, boost::shared_ptr<Foo> > TMAP;
  TMAP m(2);
  //int i = 4;

  {
    boost::shared_ptr<Foo> ptr(new Foo());
    m.set(0, ptr);
  }

  bool loop = true;
#if 1
  while (loop) {

    //for (TimerMap<int, boost::shared_ptr<int> >::Lock l(m.lock__()); l; ) {
    TIMERMAP_SYNC(TMAP, m) {
      try {
        boost::shared_ptr<Foo> ptr = m.get(0);

        xerror(0, 0, "main: 0 = %s", ptr->what());
      }
      catch (std::out_of_range &e) {
        xerror(0, 0, "main: exception: %s", e.what());
        loop = false;
        break;
      }
    }
    //sleep(2);
  }
#endif  // 0

#if 1
  {
    boost::shared_ptr<Foo> ptr(new Foo());
    m.set(1, ptr);
  }

  loop = true;
  while (loop) {
    try {
      TimerMap<int, boost::shared_ptr<Foo> >::Ptr ptr = m.getptr(1);
      xerror(0, 0, "main: 1 = %s", ptr->what());
    }
    catch (std::out_of_range &e) {
      xerror(0, 0, "main: exception: %s", e.what());
      loop = false;
    }
    //sleep(2);
  }
#endif  // 0

  //printf("%p\n", (void *)TimerMap<int, boost::shared_ptr<int> >::timer_main);
  return 0;
}
