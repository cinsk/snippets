reset
set terminal svg size 1280 720 dynamic noenhanced
set output "condwait.svg"
dx=0.7
n=3
total_box_width_relative=0.75
gap_width_relative=0.1
d_width=(gap_width_relative + total_box_width_relative) * dx / 2.
reset
set format y "%.9f"
set datafile separator comma
# set key outside right center
set key at graph 0.95, 0.95
set grid
set boxwidth total_box_width_relative/n relative
set autoscale
# set style fill transparent solid 0.5 noborder
set style fill solid 0.5 noborder
set xtics rotate
set xlabel "\namount of time given to pthread\\_cond\\_timedwait(3c)"
set ylabel "amount of exceeding(overslept) time \n measured in user-land, using clock\\_gettime(3)\n"
set title "Time blocked in pthread\\_cond\\_timedwait()"
# plot 'data-smartos.csv' using 3:xticlabels(2) with boxes, \
#      'data-lx.csv' using ($3+d_width):3 with boxes

title_sm = "SPC:SmartOS:g4-cgp-cassandra-55G - Realtime"
title_lx = "SPC:LX:g4-cpg-cassandra-55G - Monotonic"
title_aws = "AWS:i3.4xlarge - Monotonic"

plot data_smartos using 0:3:xtic(2) lc rgb"orange" with boxes title title_sm, \
     data_lx using ($0+d_width*1):3 lc rgb"light-blue" with boxes title title_lx, \
     data_aws using ($0+d_width*2):3 lc rgb"blue" with boxes title title_aws

