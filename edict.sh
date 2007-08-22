#!/bin/bash
# Copyright (c) 2007 Seong-Kook Shin <cinsk@gmail.com>
# Distributed in as-is condition. (No commercial use.)
# $Id$

ENDIC_URL="http://endic.naver.com/small_search.nhn?&query="
EEDIC_URL="http://eedic.naver.com/small.naver?where=keyword&query="
MPLAYER_PATH=mplayer
DICT_PATH=dict

# Default dictionary type
dictype=en

if which dict >& /dev/null; then
    use_dict=1
fi

if which mplayer >& /dev/null; then
    enable_audio=1
fi

function usage () {
    cat <<EOF
usage: $0 [OPTION]... WORD...
Show the definition of WORD

  -t XX    specify the dictionary type to use
     ko    - English to Korean dictionary (Naver)
     en    - English to English dictionary (Naver)

  -l       play the pronunciation audio

  -h       display this help and exit
  -V       output version information and exit

Report bugs to <cinsky@gmail.com>.
EOF
    exit 0
}

function exact_match () {
    $DICT_PATH -P - -s exact -m >/dev/null
    status=$?

    if test $staus -eq 0; then
        return 0
    elif test $status -ne 20; then
        echo "warning: dict failed with status $status" 1>&2
    fi
    return 1
}


OPTIND=0
while getopts "lt:h" opt; do
    case $opt in
        'l')
            option_listen=1
            if test -z "$enable_audio"; then
                echo "warning: mplayer not found.  (Audio diabled)" 1>&2
            fi
            ;;
        't')
            #echo "OPTARG: $OPTARG"
            dictype=$OPTARG
            ;;
        'h')
            usage
            ;;
        *)
            echo "Try \`-h' for more information." 1>&2
            exit 1
            ;;
    esac
done

if test $# -lt $OPTIND; then
    echo "error: argument required" 1>&2
    echo "Try \`-h' for more information." 1>&2
    exit 1
fi

case $dictype in
    en|EN)
        dicurl=$EEDIC_URL
        startmark='^\[dot'
        ;;
    ko|KO|kr)
        dicurl=$ENDIC_URL
        startmark='(검색결과|\[btn_close_)'
        ;;
    *)
        echo "error: unknown dictionary type \`$dictype'" 1>&2
        exit 1
        ;;
esac

shift `expr $OPTIND - 1`

for word in "$@"; do
    if test "$multi"; then echo ; fi
    w3m -dump "${dicurl}${word}" | awk "
/검색결과가 없습니다./  { exit 1 }
/$startmark/	{ doprint=1; next }
/^top$/	        { exit }
{ if (doprint) print }"
#/^\[dot/,/^top/ { print }'
#        | tail -n +2 | head -n -1
    status=$?
    #echo "STATUS: $status"
    multi=1

    if test "$status" -ne 0; then
        if test "$use_dict"; then
            $DICT_PATH -P - $word
        else
            echo "$word: not found" 1>&2
        fi
    fi

    if test "$option_listen" -a "$enable_audio" -a $status -eq 0; then
        url=`w3m -dump_source "${EEDIC_URL}${word}" | grep showproun | sed "s/[^']*'\([^']*\)'.*$/\1/"`
        echo -n "Playing..." 1>&2
        $MPLAYER_PATH -really-quiet $url
        echo "Done." 1>&2
    fi
done
