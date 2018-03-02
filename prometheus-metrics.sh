#!/bin/bash

PROMETHEUS_URL=http://165.225.137.31:9090

PROGRAM_NAME="$(basename "$0")"

error() {
    local estat="$1"
    shift
    echo "$PROGRAM_NAME: $*" 1>&2
    [ "$estat" -ne 0 ] && exit "$estat"
}

cmd_args() {
    local i=1
    echo "args: $#"
    for f in "$@"; do
        echo "arg[$i]: |$f|"
        i=$((i + 1))
    done
}

cmd_labels() {
    curl -s --url "${PROMETHEUS_URL}/api/v1/label/__name__/values" | jq -r '.data | .[]'
}

cmd_jobs() {
    curl -s --url "${PROMETHEUS_URL}/api/v1/query" --data-urlencode 'query=up' | jq -r '.data.result | map([.metric.job]) | .[] | join("")' | sort -u
}

cmd_instance() {
    local job="$1"

    [ -z "$job" ] && job=".*"
    
    # jq -r '.data.result | map([.metric.job, .metric.instance]) | ["JOB", "INSTNACE"], .[] | join(" ")'
    curl -s --url "${PROMETHEUS_URL}/api/v1/query" --data-urlencode "query=up{job=~\"${job}\"}" | jq -r '.data.result | map([.metric.job, .metric.instance]) | ["JOB", "INSTNACE"], .[] | join(" ")'
}

# curl --url 'http://165.225.137.31:9090/api/v1/query' --data-urlencode 'query=load_average{instance="kubuntu"}[5m]' | jq -r '.data.result[0].values | map([.[0], .[1] | tostring]) | ["time", "value"], .[] | join(" ")' > load.dat

# curl -s --url 'http://165.225.137.31:9090/api/v1/query' --data-urlencode 'query=load_average{instance=~".*ubuntu.*"}[5m]' | jq -r '.data.result | map(.metric.instance as $name | .values | map([ $name | tostring ] + [ (.[0] | tostring), .[1] ])) | flatten(1) | .[] | join(" ")'

if ! which jq >&/dev/null; then
    error 1 "jq(1) not found; please install it"
fi

cmd="cmd_$1"
shift
if ! declare -f "$cmd" >/dev/null; then
    error 1 "command not found - $cmd"
fi
    
cmdline=("$cmd" "$@")

"${cmdline[@]}"
