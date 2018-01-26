#!/bin/bash

PROGRAM_NAME=$(basename "$0")
BASE_DIR=$(dirname "$0")

error() {
    local ecode="$1"
    shift
    echo "$PROGRAM_NAME: $*" 1>&2
    [ "$ecode" -ne 0 ] && exit "$ecode"
}


if ! which gnuplot 2>/dev/null 2>&1; then
    error 1 "gnuplot not found"
fi

if [ "$#" -ne 3 ]; then
    echo "Usage: $PROGRAM_NAME SMARTOS-DATA LX-DATA AWS-DATA"
    error 1 "wrong number of argument(s)"
fi

gnuplot -e "data_smartos='$1'; data_lx='$2'; data_aws='$3'" "${BASE_DIR}/condwait.gnuplot"
