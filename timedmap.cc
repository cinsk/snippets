#include <boost/shared_ptr.hpp>
#include <time.h>

#include "timedmap.hh"

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

typedef timedmap<int, boost::shared_ptr<Foo> > TMAP;


int
main(void)
{
  xerror_init(0, 0);

  TMAP m(1);

  m[3] = boost::shared_ptr<Foo>(new Foo());
  sleep(2);
  m[2] = boost::shared_ptr<Foo>(new Foo());
  sleep(2);
  m[1] = boost::shared_ptr<Foo>(new Foo());

  xerror(0, 0, "getting...");
  while (true) {
    TMAP::iterator i = m.end();
    try {
// xerror(0, 0, "m[3] = %s", m[1]->what());
      xerror(0, 0, "m[3] = %s", m.at(1)->what());
    }
    catch (...) {
      xerror(0, 0, "expired");
      break;
    }
    sleep(1);
  }

  xerror(0, 0, "fin");
#if 0
  m[2] = boost::shared_ptr<Foo>(new Foo());

  xerror(0, 0, "m[2] = %s", m[2]->what());
  xerror(0, 0, "m[3] = %s", m[2]->what());
#endif  // 0

  return 0;
}

#if 0
int
main(void)
{
  xerror_init(0, 0);

  {
    TMAP m(5);

    int key = 0;
    {
      boost::shared_ptr<Foo> ptr(new Foo());
      xerror(0, 0, "---");
      m.set(key, ptr);
      xerror(0, 0, "---");
    }

    bool loop = true;

    {
      while (loop) {
        xerror(0, 0, "---");
        {
          TMAP::Ptr ptr = m.get(key);
          xerror(0, 0, "---");

          if (ptr) {
            xerror(0, 0, "main: key(%d) = %s", key, (*ptr)->what());
          }
          else {
            xerror(0, 0, "main: key(%d) was not there", key);
            break;
          }
        }
        //break;
        usleep(100000);
        //sleep(1);
      }
    }
    //m.clear();
  }
  return 0;
}
#endif  // 0
