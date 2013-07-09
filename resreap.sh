#!/bin/bash

SCRIPT=/tmp/awk.$$
LOG_FILE=/var/log/resreap.log

# if a process has equal to or greater than MAX_COUNT CLOSE_WAIT socket(s),
# it will be killed
MAX_COUNT=2

# reaping job occurrs at every INTERVAL second(s)
INTERVAL=5

SIGNAL=TERM
SIGWAIT=2

PROGNAME=`basename $0`
VERSION_STRING=0.1

function usage_and_exit() {
    cat <<EOF
Kill processes that have enough CLOSE_WAIT socket(s)
Usage: $PROGNAME [OPTION...]

    -l N     If a process has more than or equal to N CLOSE_WAIT socket(s),
             it will be killed with a signal (default: $MAX_COUNT)

    -i N     Set sleep interval between checks in seconds (default: $INTERVAL)

    -s SIG   Set the signal name (e.g. TERM or KILL) that will be send to a process
             (default: $SIGNAL)
    -w SEC   Set the waiting time in seconds between the signal and SIGKILL
             (default: $SIGWAIT)

    -h       show this poor help messages and exit
    -v       show version information and exit

Note that if a process receives the signal, and the process is alive for $SIGWAIT
second(s), the process will receive SIGKILL.  

EOF
    exit 0
}

function version_and_exit() {
    echo "$PROGNAME version $VERSION_STRING"
    exit 0
}

while getopts hvs:i:l:w: opt; do
    case $opt in
        l)
            MAX_COUNT=$OPTARG
            ;;
        w)
            SIGWAIT=$OPTARG
            ;;
        i)
            INTERVAL=$OPTARG
            ;;
        s)
            SIGNAL=$OPTARG
            ;;
        h)
            usage_and_exit
            ;;
        v)
            version_and_exit
            ;;
        ?)
            echo "usage: `basename $0` [OPTION...]"
            exit 1
            ;;
    esac
done
shift $(($OPTIND - 1))

echo "limits: $MAX_COUNT"
echo "interval: $INTERVAL"
echo "signal: $SIGNAL"

trap "rm -f \"$SCRIPT\"; exit 1;" SIGINT SIGTERM SIGHUP SIGQUIT

cat > $SCRIPT <<EOF
{
    m = match(\$9, /([0-9][0-9]*)\/[^/]*/, ary); 
    if (m != 0)   
        sum[ary[1]] += 1;
}

END {
    for (k in sum) { 
        print k, sum[k]
    }
}
EOF

function log() {
    echo "$PROGNAME: $@" >> $LOG_FILE
    echo "$PROGNAME: $@" 1>&2
    return 0
}

function log_noprog() {
    echo "$@" >> $LOG_FILE
    echo "$@" 1>&2
    return 0
}

function xkill() {
    pid=$1
    (echo kill -$SIGNAL "$pid";
        sleep "$SIGWAIT";
        kill -0 "$pid" >&/dev/null && \
            log "process $pid didn't exit, force killing" && \
            echo kill -9 "$pid";)&
    return 0
}

while true; do
    PCOUNT=0
    #netstat -p --numeric-ports -e | grep CLOSE_WAIT | awk -f "$SCRIPT" 
    netstat -p --numeric-ports -e 2>/dev/null| grep CLOSE_WAIT | awk -f "$SCRIPT" | \
        while read line; do
        pid=`echo "$line" | awk '{ print $1 }'`;
        count=`echo "$line" | awk '{ print $2}'`;
        if test "$count" -ge 2; then
            if test "$PCOUNT" -eq 0; then
                PCOUNT=1
                log_noprog "# `date -u -Iseconds`"
            fi
            #(echo -e \tkill -$SIGNAL "$pid"; sleep "$SIGWAIT"; kill -0 "$pid" && echo -e \tkill -9 "$pid")&
            xkill "$pid"
            log "process $pid has more than $count CLOSE_WAIT socket(s), killed"
        fi;
    done
    sleep $INTERVAL
done

