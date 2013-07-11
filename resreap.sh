#!/bin/bash

SCRIPT=/tmp/awk.$$
LOG_FILE=/var/log/resreap.log
PID_FILE=/var/run/resreap.pid

# if a process has equal to or greater than MAX_COUNT CLOSE_WAIT socket(s),
# it will be killed
MAX_COUNT=2

# reaping job occurrs at every INTERVAL second(s)
INTERVAL=5

SIGNAL=TERM
SIGWAIT=2


DATE=${DATE:-$(which date 2>/dev/null)}
NETSTAT=${NETSTAT:-$(which netstat 2>/dev/null)}
FUSER=${FUSER:-$(which fuser 2>/dev/null)}
AWK=${AWK:-$(which awk 2>/dev/null)}

PROGNAME=`basename $0`
VERSION_STRING=0.1

FILTER=""


function usage_and_exit() {
    cat <<EOF
Kill processes that have enough CLOSE_WAIT socket(s)
Usage: $PROGNAME [OPTION...]

    -f PAT      Kill only processes whose command matches PAT

    -l N        If a process has more than or equal to N CLOSE_WAIT
                socket(s), it will be killed with a signal (default:
                $MAX_COUNT)

    -i N        Set sleep interval between checks in seconds
                (default: $INTERVAL)

    -c CMD      Before sending a signal, execute CMD in the shell,
                If this CMD returns non-zero returns, the process
                will not receive any signal.
 
    -s SIG      Set the signal name (e.g. TERM or KILL) that will be send
                to a process (default: $SIGNAL)
    -w SEC      Set the waiting time in seconds between the signal and
                SIGKILL (default: $SIGWAIT)

    -d          dry run, no kill
    -D          debug mode

    -h          show this poor help messages and exit
    -v          show version information and exit

Note that if a process receives the signal, and the process is alive
for $SIGWAIT second(s), the process will receive SIGKILL.

If you are going to use "-f" option, I recommend to try "-d -D" option
first.  If you get the pid of the culprit process, try to get the
command name by "ps -p PID -o command=" where PID is the pid of that
process.

You could send two signal(s) before sending SIGKILL using '-S' option.
This can be useful since some JVM print stacktrace on SIGQUIT.
EOF
    exit 0
}


function version_and_exit() {
    echo "$PROGNAME version $VERSION_STRING"
    exit 0
}


function check_environ() {
    if [ ! -x "$NETSTAT" ]; then
        error 1 "netstat(1) not found"
    fi

    if [ ! -x "$FUSER" ]; then
        error 1 "fuser(1) not found"
    fi

    if [ ! -x "$AWK" ]; then
        error 1 "awk(1) not found"
    fi

    if [ ! -x "$DATE" ]; then
        error 1 "date(1) not found"
    fi
}


function error() {
    # Usage: log MESSAGE
    code=$1
    shift
    echo "$PROGNAME: $@" 1>&2
    if [ "$code" -ne 0 ]; then
        exit $code;
    fi
    return 0
}


function log() {
    # Usage: log MESSAGE
    echo -e "$PROGNAME: $@" >> $LOG_FILE
    echo -e "$PROGNAME: $@" 1>&2
    return 0
}


function log_noprog() {
    # Usage: log_noprog MESSAGE
    echo -e "$@" >> $LOG_FILE
    echo -e "$@" 1>&2
    return 0
}


function debug() {
    # Usage: debug MESSAGE
    [ -n "$DEBUG_MODE" ] && echo -e "$PROGNAME: $@" 1>&2
    return 0
}


function check_rights() {
    # Usage: check_rights PID
    pid=$1
    euid=$(id -u)
    if test "$euid" -ne 0 -a "$euid" -ne "$(ps -p $pid -o euid=)"; then
        debug "didn't have enough access rights to kill process $pid"
        return 1;
    fi

    echo "$(ps -p $pid -o command=)" | grep "$FILTER" >/dev/null
    ret=$?
    if test "$ret" -ne 0; then
        debug "process $pid does not match to the filter, skipped"
    fi
    return $ret
}


function xkill() {
    pid=$1

    (   [ -n "$USRCMD" ] && if ! eval "( exec >>\"$LOG_FILE\" 2>&1; $USRCMD )"; then exit $?; fi
        kill -$SIGNAL "$pid";
        sleep "$SIGWAIT";
        kill -0 "$pid" >&/dev/null && \
            log "process $pid didn't exit, force killing" && \
            kill -9 "$pid";)&
    return 0
}


check_environ

while getopts hvdf:Ds:i:l:w:c: opt; do
    case $opt in
        f)
            FILTER="$OPTARG"
            ;;
        l)
            MAX_COUNT=$OPTARG
            ;;
        D)
            DEBUG_MODE=1
            ;;
        d)
            DRY_RUN=1
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
        c)
            USRCMD=$OPTARG
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


debug "limits: $MAX_COUNT"
debug "interval: $INTERVAL"
debug "signal: $SIGNAL"
debug "filter: |$FILTER|"

trap "rm -f \"$PID_FILE\" \"$SCRIPT\"; exit 1;" SIGINT SIGTERM SIGHUP SIGQUIT

echo "$$" > "$PID_FILE"

#
# "netstat -t -p --numeric-ports -e" will print something like these:
#
# tcp        0      0 ip-10-149-8-221.ec2.i:40165 ec2-54-225-237-102.com:6379 CLOSE_WAIT  root       29692      4402/java           
# tcp        1      0 ip-10-149-8-221.ec2.i:10011 ip-10-83-118-89.ec2.i:48912 CLOSE_WAIT  root       0          -
#
# As in above, in second line, the line did not have PID/COMMAND in
# the last fields ($9).  Here, the internal port is 10011, and
# fuser(1) can give the proper PID when executed with "-n tcp 10011".
#

#
# Sometimes, fuser(1) output will be either of
# 
# $ fuser -n tcp 10010
# 10010/tcp:            2285
#
# $ fuser -n tcp 10010
# 10010/tcp:            2285  3540  3548
#
cat > $SCRIPT <<EOF
/^tcp/ {
    m = match(\$9, /([0-9][0-9]*)\/[^/]*/, ary); 
    if (m != 0) {
        # handle "port/command" value in the last field.
        sum[ary[1]] += 1;
    }
    else {
        if (\$9 == "-") {
            # handle if the last field is "-"
            m = match(\$4, /[^:]*:([0-9][0-9]*)/, ary);
            if (m != 0) {
                port = ary[1];
                "fuser -n tcp " port " 2>/dev/null" | getline pid
                # extract the first process from fuser output
                m = match(pid, /[ \t]*([0-9]+).*/, ary)
                if (m != 0) {
                    #printf "port(%s) belongs to process(%s)\n", port, ary[1]
                    sum[ary[1]] += 1
                }
            }
        }
    }
}

END {
    for (k in sum) { 
        # output "PID COUNT" where COUNT is the # of CLOSE_WAIT socket(s)
        print k, sum[k]
    }
}
EOF


#declare -a CWCOUNT

while true; do
    PCOUNT=0
    #netstat -p --numeric-ports -e | grep CLOSE_WAIT | awk -f "$SCRIPT" 
    debug "--"
    netstat -t -p --numeric-ports -e | \
        grep CLOSE_WAIT | awk -f "$SCRIPT" | \

        while read line; do
        pid=`echo "$line" | awk '{ print $1 }'`;
        # 'count' holds the count for CLOSE_WAIT socket(s)
        count=`echo "$line" | awk '{ print $2}'`;

        debug "process[$pid] = $count"

        if test "$count" -ge "$MAX_COUNT"; then
            if test "$PCOUNT" -eq 0; then
                PCOUNT=1
                log_noprog "# `date -u -Iseconds`"
            fi

            if check_rights "$pid"; then
                cmd=$(ps -p $pid -o command=)
                if [ -z "$DRY_RUN" ]; then
                    xkill "$pid"
                    log "process $pid has more than $count CLOSE_WAIT socket(s), killed\n  command: $cmd"
                else
                    log "process $pid has more than $count CLOSE_WAIT socket(s), dry run\n  command: $cmd"
                fi
            fi
        fi;
    done
    sleep $INTERVAL
done

