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

  df_t df;

  DF(df) {
    df_t df2;

    ::sleep(1);
    DF(df2) {
      ::sleep(1);
    }
    printf("diff2: %lu\n", df2.value);
  }

  printf("diff: %lu\n", df.value);

  return 0;
}
