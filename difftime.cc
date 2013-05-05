#include "difftime.h"

/*
DIFFTIME {
  hello();
  blah;
} END_DIFFTIME;
*/


#include <cstdio>
#include <functional>
#include <unistd.h>

struct dfout : public std::unary_function<long, void> {
  void operator() (long diff) {
    printf("dfout: diff: %ld\n", diff);
  }
};


int
main(void)
{
  {
    long diff;
    DIFFTIME(diff) {
      printf("work\n");
      ::sleep(1);
    }
    printf("DIFF: %ld\n", diff);
  }

  {
    DiffTime<dfout> df;
    printf("hello\n");
    ::sleep(1);
  }

  long diff;

  DIFFTIME_BEGIN {
    long diff2;
    ::sleep(1);
    DIFFTIME_BEGIN {
      ::sleep(1);
    } END_DIFFTIME(diff2);
    printf("diff2: %lu\n", diff2);
  } END_DIFFTIME(diff);

  printf("diff: %lu\n", diff);

  return 0;
}
