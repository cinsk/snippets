reset
#plot for [instance in "kubuntu ubuntu"] "tmp.data" using 2:($1==instance?$3:1/0) with linespoint

set timefmt "%s"
set format x "%H:%M:%S"
set xdata time

plot for [instance in "`cut -d' ' -f1 tmp.data | sort -u | tr '\n' ' '`"] sprintf("<awk '$1 == \"%s\" { print $0 }' tmp.data", instance) using 2:3 with lines title sprintf("host - %s", instance)
#plot for [instance in "`echo -e 'kubuntu\nubuntu' | xargs echo`"] sprintf("<awk '$1 == \"%s\" { print $0 }' tmp.data", instance) using 2:3 with linespoint title sprintf("host - %s", instance)
#plot for [instance in "kubuntu ubuntu"] sprintf("<awk '$1 == \"%s\" { print $0 }' tmp.data", instance) using 2:3 with linespoint title sprintf("host - %s", instance)

# plot sprintf("tmp.data") using 2:3 with linespoint

#set y2tics
#plot \
#     "<awk '$1 == \"ubuntu\" { print $0 }' tmp.data" using 2:3 with linespoint title "ubuntu" axes x1y1, \
#     "<awk '$1 == \"kubuntu\" { print $0 }' tmp.data" using 2:3 with linespoint title "kubuntu" axes x1y2
