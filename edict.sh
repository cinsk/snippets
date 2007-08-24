#!/bin/bash
# Copyright (c) 2007 Seong-Kook Shin <cinsk@gmail.com>
# Distributed in as-is condition. (No commercial use.)
# $Id$

ENDIC_URL="http://endic.naver.com/small_search.nhn?&query="
EEDIC_URL="http://eedic.naver.com/small.naver?where=keyword&query="

W3M_PATH=`which w3m 2>/dev/null`
MPLAYER_PATH=`which mplayer 2>/dev/null`
DICT_PATH=`which dict 2>/dev/null`

PROGRAM_NAME="edict"
REVISION_STR='$Revision$'

# Default dictionary type
dictype=en

if test ! -x "$W3M_PATH"; then
    echo "error: w3m(http://w3m.sourceforge.net/) not found." 1>&2
    echo "error: Perhaps your system does not have w3m installed." 1>&2
    echo "error: Or try to adjust the value of W3M_PATH in this script." 1>&2
    exit 1
fi

if test -x "$DICT_PATH"; then
    use_dict=1
fi

if test -x "$MPLAYER_PATH"; then
    enable_audio=1
fi

repeat_count=1

function usage () {
    cat <<EOF
usage: $PROGRAM_NAME [OPTION]... WORD...
Show the definition of WORD

  -t XX    specify the dictionary type to use
     ko    - English to Korean dictionary (Naver)
     en    - English to English dictionary (Naver)

  -l       play the pronunciation audio
  -n XX	   set repeat count of the audio play

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
while getopts "lt:hVn:" opt; do
    case $opt in
        'l')
            option_listen=1
            if test -z "$enable_audio"; then
                echo "warning: mplayer not found.  (Audio diabled)" 1>&2
            fi
            ;;
        'n')
            repeat_count=$OPTARG
            ;;
        't')
            #echo "OPTARG: $OPTARG"
            dictype=$OPTARG
            ;;
        'h')
            usage
            ;;
        'V')
            echo "$PROGRAM_NAME $REVISION_STR" | \
                sed -e 's/\$//g' | sed -e 's/Revision: \([0-9.]*\) /version \1/'
            exit 0
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
        dictname="Collins COBUILD Advanced Learner's English Dictionary 4th Edition"
        ;;
    ko|KO|kr)
        dicurl=$ENDIC_URL
        startmark='(검색결과|\[btn_close_)'
        dictname="동아 프라임 영한사전"
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
            exit $?
        else
            echo "$word: not found" 1>&2
            exit 1
        fi
    fi

    if test "$option_listen" -a "$enable_audio" -a $status -eq 0; then
        url=`w3m -dump_source "${EEDIC_URL}${word}" | grep showproun | sed "s/[^']*'\([^']*\)'.*$/\1/"`
        while test "$repeat_count" -gt 0; do
            echo -n "Playing..." 1>&2
            $MPLAYER_PATH -really-quiet $url
            echo "Done." 1>&2
            repeat_count=`expr $repeat_count - 1`
        done
    fi
    echo -e "\n--"
    echo "$dictname"
    echo "A courtesy of Naver dictionary service (http://dic.naver.com/)"
done
