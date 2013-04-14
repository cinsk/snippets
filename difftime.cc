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
    printf("diff: %ld\n", diff);
  }
};


int
main(void)
{
  //struct timeval old, now;
  long diff;

  //gettimeofday(&old, NULL);
  {
    DiffTime<dfout> df;
    printf("hello\n");
    ::sleep(1);
  }

  //return 0;

  DIFFTIME {
    long diff2;
    ::sleep(1);
    DIFFTIME {
      ::sleep(1);
    } END_DIFFTIME(diff2);
    printf("diff2: %lu\n", diff2);
  } END_DIFFTIME(diff);

  //gettimeofday(&now, NULL);

  //printf("%lu\n", diff_timeval(&now, &old));
  printf("diff: %lu\n", diff);

  return 0;
}
