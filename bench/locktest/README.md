
Build
=====

    $ gcc -pthread locktest.c -lpthread

Usage
=====

    $ ./a.out -c 100 -t 10      # Run for 10 seconds with 100 threads for lock/increase the counter.
    $ ./a.out -c 100 -l 100000  # Run until the counter reaches 100000 with 100 threads.
    $ ./a.out                   # run forever
