#!/bin/bash

# Some distribution places redis-server(1) and redis-sentinel(1) in /usr/sbin/
export PATH=/usr/sbin:/sbin:$PATH

PREFIX=$HOME/redis-replica

REDIS_SERVER=$(which redis-server 2>/dev/null)
REDIS_SENTINEL=$(which redis-sentinel 2>/dev/null)
REDIS_CLI=$(which redis-cli 2>/dev/null)
REDIS_BASE_PORT=6379
SENTINEL_BASE_PORT=26379

REDIS_ROOT=$PREFIX/root
SENTINEL_ROOT=$PREFIX/sentinels
REDIS_COMMON_CONF=$PREFIX/etc/redis-default.conf
REDIS_NODE_CONF_TMPL=$REDIS_ROOT/%d/redis.conf

REDIS_SLAVES=2
SENTINELS=1

CONF_BIND_ADDR="127.0.0.1"
CONF_TIMEOUT=0
CONF_TCP_KEEPALIVE=0
CONF_LOGLEVEL=notice
CONF_DATABASES=4
CONF_SERVER_STALE_DATA=no
CONF_SLAVE_READ_ONLY=yes

SCREEN=`which screen 2>/dev/null`
SCREEN_SESSION=redis

crlf=$'\r\n'

function endpoint () {
    local opt def_host def_port h p OPTARG OPTIND s
    while getopts h:p: opt "$@"; do
        case $opt in
            h)
                def_host=$OPTARG
                ;;
            p)
                def_port=$OPTARG
                #echo "default port: $def_port"
                ;;
        esac
    done
    shift $(($OPTIND - 1))

    if [ "$#" -ne 3 ]; then
        echo "usage: endpoint [-h host] [-p port] ENDPOINT-STRING HOST PORT"
        return 1
    fi

    h=$(echo "$1" | awk -F: '{ print $1 }')
    p=$(echo "$1" | awk -F: '{ print $2 }')

    h=${h:-$def_host}
    p=${p:-$def_port}

    #echo "host: $h"
    #echo "port: $p"

    s="$2=\"$h\"; $3=\"$p\""
    eval "$s"
    #echo "$s"
    return 0
}
export -f endpoint

function redis_join () {
    local opt host port OPTIND OPTARG do_master

    realpath=$(which realpath 2>/dev/null)
    if [ -z "$realpath" ]; then
        realpath=$(which grealpath 2>/dev/null)
    fi
    if [ -z "$realpath" ]; then
        echo "redis_join: cannot find realpath(1)" 1>&2
        return 1
    fi

    PATH=/usr/sbin:/sbin:$PATH
    server=$(which redis-server 2>/dev/null)

    OPTIND=1
    do_master=0
    while getopts "e:c:mh" opt; do
        case "$opt" in
            e)
                echo "OPTARG: $OPTARG" 1>&2
                endpoint -p 26379 "$OPTARG" host port
                #eval "$s"
                ;;
            m)
                do_master=1
                ;;
            c)
                config=$($realpath $OPTARG)
                ;;
            h)
                cat <<EOF
usage: redis_join [OPTION...] MASTER-NAME CONF

  -e HOST:PORT   Set the sentinel host and port (required)
  -c CONFIG      Use the redis configuration file, CONFIG

  -m             At the startup time, consider to create a master.


EOF
                return 0
                ;;
            ?)
                echo "redis_join: invalid argument, try \`-h'" 1>&2
                return 1
                ;;
        esac
    done
    shift $(($OPTIND - 1))

    if [ $# -lt 2 ]; then
        echo "redis_join: argument(s) required, try \`-h'" 1>&2
        return 1
    fi

    master=$1
    conf=$2

    saddr=$host
    sport=$port

    while true; do
        minfo=`redis-cli -h $saddr -p $sport sentinel get-master-addr-by-name "$master"`
        if [ "$?" -ne 0 ]; then
            echo "redis_join: can't connect to the sentinel" 1>&2
            return 1;
        fi
        maddr=`echo "$minfo" | head -1`
        mport=`echo "$minfo" | tail -1`

        echo "master addr: |$maddr|"
        echo "master port: |$mport|"

        if echo "$maddr" | grep IDONTKNOW >&/dev/null; then
	    echo "NO MASTER INFO IN SENTINEL: do_master($do_master)"
            if [ "$do_master" -ne 0 ]; then
                do_master=0
                tmp=/tmp/redis-conf.$$
                trap "rm $tmp; return 1" INT HUP TERM 
                sed -e 's/^slaveof.*$//g' "$conf" >"$tmp"
                mv "$tmp" "$conf"
                trap - INT HUP TERM
                echo "Starting master"
                $server "$conf"
		continue
            else
                echo "error: master, $master does not exist"
                return 1;
            fi
        fi

        if ! nc -z $maddr $mport; then
            echo "$master($maddr:$mport) seems down, wait..."
            sleep 0.5
            continue
        fi

        echo "maddr: $maddr"
        echo "mport: $mport"

        if ! redis-cli -h $maddr -p $mport INFO 2>/dev/null | grep ^role | grep master >&/dev/null; then
            echo "$master($maddr:$mport) is actually a slave. try again..."
            sleep 0.5
            continue
        fi

        tmp=/tmp/redis-conf.$$
        trap "rm $tmp; return 1" INT HUP TERM 
        sed -e "s/^slaveof.*$//g" "$conf" >"$tmp"
        sed -i -e "\$i\\
slaveof $maddr $mport" "$tmp"
        mv "$tmp" "$conf"
        trap - INT HUP TERM
        cat "$conf"
        $server "$conf"
    done
}
export -f redis_join

function create_base_conf() {
    if [ ! -r "$REDIS_COMMON_CONF" ]; then
        mkdir -p `dirname "$REDIS_COMMON_CONF"`
        cat >"$REDIS_COMMON_CONF" <<EOF
daemonize no
bind $CONF_BIND_ADDR
timeout $CONF_TIMEOUT
tcp-keepalive $CONF_TCP_KEEPALIVE
loglevel $CONF_LOGLEVEL
logfile stdout
databases $CONF_DATABASES
save 900 1
save 300 10
save 60 10000
dbfilename dump.rdb
slave-serve-stale-data $CONF_SERVER_STALE_DATA
slave-read-only $CONF_SLAVE_READ_ONLY
EOF
    fi
}

function create_node_conf() {
    for f in `seq 0 $REDIS_SLAVES`; do
        nodebase=$REDIS_ROOT/$f
        if [ ! -d "$nodebase" ]; then
            mkdir -p "$nodebase"
        fi
        nodeconf=$nodebase/redis.conf

        if [ ! -f "$nodeconf" ]; then
            cat >"$nodeconf" <<EOF
include $REDIS_COMMON_CONF
pidfile $nodebase/redis.pid
port $((REDIS_BASE_PORT + f))
dir $nodebase

EOF
            if [ "$f" -ne 0 ]; then
                echo "slaveof 127.0.0.1 $REDIS_BASE_PORT" >> "$nodeconf"
            fi
        fi
    done
}


function create_sentinel_conf() {
    for f in `seq 1 $SENTINELS`; do
        nodebase=$SENTINEL_ROOT/$f
        if [ ! -d "$nodebase" ]; then
            mkdir -p "$nodebase"
        fi
        nodeconf=$nodebase/sentinel.conf

        if [ ! -f "$nodeconf" ]; then
            master="master0"
            cat >"$nodeconf" <<EOF
sentinel monitor $master 127.0.0.1 $REDIS_BASE_PORT $SENTINELS
sentinel down-after-milliseconds $master 1000
sentinel failover-timeout $master 600000
sentinel can-failover $master yes
sentinel parallel-syncs $master 1
EOF
        fi
    done
}

function session_alive() {
    s=`$SCREEN -list | grep ^$'\t' | grep $SCREEN_SESSION`
    if test -z "$s"; then
        return 1;
    else
        return 0;
    fi
}

function create_session() {
    if ! session_alive; then
        window=0
        $SCREEN -dmS $SCREEN_SESSION

        nodebase=$REDIS_ROOT/0
        nodeconf=$nodebase/redis.conf
        $SCREEN -r $SCREEN_SESSION -p $window -X title "MASTER:$REDIS_BASE_PORT"
        $SCREEN -r $SCREEN_SESSION -p $window -X stuff "$REDIS_SERVER ${nodeconf}${crlf}"

        $SCREEN -r $SCREEN_SESSION -X screen
        window=$((window + 1))
        $SCREEN -r $SCREEN_SESSION -p $window -X title "MASTER-MONITOR:$REDIS_BASE_PORT"
        $SCREEN -r $SCREEN_SESSION -p $window -X stuff "sleep 1; $REDIS_CLI -p $REDIS_BASE_PORT MONITOR${crlf}"

        $SCREEN -r $SCREEN_SESSION -X screen
        window=$((window + 1))
        $SCREEN -r $SCREEN_SESSION -p $window -X title "MASTER-CLIENT:$REDIS_BASE_PORT"
        $SCREEN -r $SCREEN_SESSION -p $window -X stuff "sleep 1; $REDIS_CLI -p $REDIS_BASE_PORT ${crlf}"


        port=$REDIS_BASE_PORT
        for f in `seq 1 $REDIS_SLAVES`; do
            nodebase=$REDIS_ROOT/$f
            nodeconf=$nodebase/redis.conf
            
            $SCREEN -r $SCREEN_SESSION -X screen
            window=$((window + 1))
            $SCREEN -r $SCREEN_SESSION -p $window -X title "SLAVE#$f:$((port + f))"
            $SCREEN -r $SCREEN_SESSION -p $window -X stuff "$REDIS_SERVER ${nodeconf}${crlf}"

            $SCREEN -r $SCREEN_SESSION -X screen
            window=$((window + 1))
            $SCREEN -r $SCREEN_SESSION -p $window -X title "SLAVE#$f-MONITOR:$((port + f))"
            $SCREEN -r $SCREEN_SESSION -p $window -X stuff "sleep 1; $REDIS_CLI -p $((f + REDIS_BASE_PORT)) MONITOR${crlf}"

            $SCREEN -r $SCREEN_SESSION -X screen
            window=$((window + 1))
            $SCREEN -r $SCREEN_SESSION -p $window -X title "SLAVE#$f-CLIENT:$((port + f))"
            $SCREEN -r $SCREEN_SESSION -p $window -X stuff "sleep 1; $REDIS_CLI -p $((f + REDIS_BASE_PORT))${crlf}"
        done

        port=$SENTINEL_BASE_PORT
        for f in `seq 1 $SENTINELS`; do
            nodebase=$SENTINEL_ROOT/$f
            nodeconf=$nodebase/sentinel.conf

            $SCREEN -r $SCREEN_SESSION -X screen
            window=$((window + 1))
            $SCREEN -r $SCREEN_SESSION -p $window -X title "SENTINEL#$f:$((port + f - 1))"
            $SCREEN -r $SCREEN_SESSION -p $window -X stuff "$REDIS_SENTINEL ${nodeconf} --port $((port + f - 1)) ${crlf}"

            $SCREEN -r $SCREEN_SESSION -X screen
            window=$((window + 1))
            $SCREEN -r $SCREEN_SESSION -p $window -X title "SENTINEL#$f-MONITOR:$((port + f - 1))"
            $SCREEN -r $SCREEN_SESSION -p $window -X stuff "sleep 1; $REDIS_CLI -p $((port + f - 1)) PSUBSCRIBE '*' ${crlf}"

            $SCREEN -r $SCREEN_SESSION -X screen
            window=$((window + 1))
            $SCREEN -r $SCREEN_SESSION -p $window -X title "SENTINEL#$f-CLIENT:$((port + f - 1))"
            $SCREEN -r $SCREEN_SESSION -p $window -X stuff "sleep 1; $REDIS_CLI -p $((port + f - 1))${crlf}"
        done

        return 0
    else
        return 1;
    fi
}

rm -rf "$PREFIX"

create_base_conf
create_node_conf
create_sentinel_conf


create_session
