
Build
=====

    $ gcc -pthread condwait.c -lm -lpthread

Chart
=====

On each type of machine, run `./a.out > data.csv`, gather 3 CSV files, and feed them to `mkchart.sh`:

    $ ./mkchart.sh data-smartos.csv data-lx.csv data-aws.csv
    
