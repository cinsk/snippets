
Build
=====

SmartOS:

    $ gcc -pthread condwait.c -lm -lpthread

LX, KVM Linux, or AWS Linux:

    $ gcc -pthread -DUSE_MONOTONIC -DCOND_SETCLOCK condwait.c -lm -lpthread
    
Chart
=====

On each type of machine, run `./a.out > data.csv`, gather 3 CSV files, and feed them to `mkchart.sh`:

    $ ./mkchart.sh data-smartos.csv data-lx.csv data-aws.csv
    
