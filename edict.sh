#!/bin/bash
# Copyright (c) 2007 Seong-Kook Shin <cinsk@gmail.com>
# Distributed in as-is condition. (No commercial use.)
# $Id$

ENDIC_URL="http://endic.naver.com/small_search.nhn?&query="
EEDIC_URL="http://eedic.naver.com/small.naver?where=keyword&query="

#
# bug: lie, bank 
# 
W3M_PATH=`which w3m 2>/dev/null`
MPLAYER_PATH=`which mplayer 2>/dev/null`
DICT_PATH=`which dict 2>/dev/null`
TPUT_PATH=`which tput`

PRINTF_PATH=`which printf`

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
                echo "warning: mplayer not found.  (Audio disabled)" 1>&2
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
        #startmark='^\[dot'
        startmark='^닫기$'
        dictname="Collins COBUILD Advanced Learner's English Dictionary 4th Edition"
        ;;
    ko|KO|kr)
        dicurl=$ENDIC_URL
        #startmark='(검색결과|\[btn_close_)'
        startmark='^닫기$'
        dictname="동아 프라임 영한사전"
        ;;
    *)
        echo "error: unknown dictionary type \`$dictype'" 1>&2
        exit 1
        ;;
esac

function underline() {
    $TPUT_PATH smul
    echo -n "$@"
    $TPUT_PATH rmul
}

function italic() {
    $TPUT_PATH sitm
    echo -n "$@"
    $TPUT_PATH ritm
}

#
# Unicode character map for the image glyphs
#
blt_exam=`echo -e '\xe2\x96\xb6'` # U+25B6
blt_05=`$PRINTF_PATH '\u2192'`     # rightwards arrow
bu_tr13=`echo -e '\xe2\x96\xb6'`  # U+25B6
ico_star1=`echo -e '\xe2\x98\x85'` # U+2605
ico_star2="${ico_star1}${ico_star1}"
ico_star3="${ico_star1}${ico_star1}${ico_star1}"
ico_star4="${ico_star1}${ico_star1}${ico_star1}${ico_star1}"
ico_star5="${ico_star1}${ico_star1}${ico_star1}${ico_star1}${ico_star1}"

phonetic_accent=`$PRINTF_PATH '\u02c8'`

phonetic_tt=`$PRINTF_PATH '\u02d0'`
phonetic_tt_u=`underline ${phonetic_tt}`

phonetic_ga=`$PRINTF_PATH '\u0251'`
phonetic_ga_u=`underline ${phonetic_ga}`
phonetic_ga_em=`italic ${phonetic_ga}`

phonetic_ge=`$PRINTF_PATH '\u0259'`
phonetic_ge_u=`underline ${phonetic_ge}`
phonetic_ge_em=`italic ${phonetic_ge}`
phonetic_ge_up=`$PRINTF_PATH '\u1d4a'`

phonetic_se=`$PRINTF_PATH '\u025c'`
phonetic_se_u=`underline ${phonetic_se}`
phonetic_se_em=`italic ${phonetic_se}`

phonetic_go=`$PRINTF_PATH '\u0252'`
phonetic_go_u=`underline ${phonetic_go}`
phonetic_go_em=`italic ${phonetic_go}`

phonetic_r_up=`$PRINTF_PATH '\u02b3'`

phonetic_ii=`$PRINTF_PATH '\u026A'`
phonetic_ii_u=`underline ${phonetic_ii}`
phonetic_ii_em=`italic ${phonetic_ii}`

phonetic_ia=`$PRINTF_PATH '\u00e6'`
phonetic_ia_u=`underline ${phonetic_ia}`
phonetic_ia_em=`italic ${phonetic_ia}`

phonetic_is=`$PRINTF_PATH '\u0283'`
phonetic_is_u=`underline ${phonetic_is}`
phonetic_is_em=`italic ${phonetic_is}`

phonetic_in=`$PRINTF_PATH '\u014b'`
phonetic_in_u=`underline ${phonetic_in}`
phonetic_in_em=`italic ${phonetic_in}`

phonetic_gu=`$PRINTF_PATH '\u028c'`
phonetic_gu_u=`underline ${phonetic_gu}`
phonetic_gu_em=`italic ${phonetic_gu}`

phonetic_io=`$PRINTF_PATH '\u0254'`
phonetic_io_u=`underline ${phonetic_io}`
phonetic_io_em=`italic ${phonetic_io}`

phonetic_iu=`$PRINTF_PATH '\u028a'`
phonetic_iu_u=`underline ${phonetic_iu}`
phonetic_iu_em=`italic ${phonetic_iu}`

phonetic_id=`$PRINTF_PATH '\u00f0'`
phonetic_id_u=`underline ${phonetic_id}`
phonetic_id_em=`italic ${phonetic_id}`

phonetic_iz=`$PRINTF_PATH '\u0292'`
phonetic_iz_u=`underline ${phonetic_iz}`
phonetic_iz_em=`italic ${phonetic_iz}`

phonetic_it=`$PRINTF_PATH '\u03b8'`
phonetic_it_u=`underline ${phonetic_it}`
phonetic_it_em=`italic ${phonetic_it}`

shift `expr $OPTIND - 1`

for word in "$@"; do
    if test "$multi"; then echo ; fi
    #echo "URL: ${dicurl}${word}"
    $W3M_PATH -ppc 4 -cols 180 -dump "${dicurl}${word}" | awk "
/검색결과가 없습니다./  { print \"@notfound@\"; exit 1 }
/$startmark/	{ doprint=1; next }
/^top$/	        { exit }
{ if (doprint) print }" | sed -ne "
s/\[blt_exam\]/${blt_exam}/g
s/\[blt_05\]/${blt_05}/g
s/\[ico_star1\]/${ico_star1}/g
s/\[ico_star2\]/${ico_star2}/g
s/\[ico_star3\]/${ico_star3}/g
s/\[ico_star4\]/${ico_star4}/g
s/\[ico_star5\]/${ico_star5}/g
s/\[bu_tr13\]/${bu_tr13}/g
s/\[ga\]/${phonetic_ga}/g
s/\[ga_u\]/${phonetic_ga_u}/g
s/\[ga_em\]/${phonetic_ga_em}/g
s/\[ge\]/${phonetic_ge}/g
s/\[ge_u\]/${phonetic_ge_u}/g
s/\[ge_em\]/${phonetic_ge_em}/g
s/\[ge_up\]/${phonetic_ge_up}/g
s/\[se\]/${phonetic_se}/g
s/\[se_u\]/${phonetic_se_u}/g
s/\[se_em\]/${phonetic_se_em}/g
s/\[tt\]/${phonetic_tt}/g
s/\[tt_u\]/${phonetic_tt_u}/g
s/\[r_up\]/${phonetic_r_up}/g
s/\[ii\]/${phonetic_ii}/g
s/\[ii_u\]/${phonetic_ii_u}/g
s/\[ii_em\]/${phonetic_ii_em}/g
s/\[ia\]/${phonetic_ia}/g
s/\[ia_u\]/${phonetic_ia_u}/g
s/\[ia_em\]/${phonetic_ia_em}/g
s/\[is\]/${phonetic_is}/g
s/\[is_u\]/${phonetic_is_u}/g
s/\[is_em\]/${phonetic_is_em}/g
s/\[in\]/${phonetic_in}/g
s/\[in_u\]/${phonetic_in_u}/g
s/\[in_em\]/${phonetic_in_em}/g
s/\[go\]/${phonetic_go}/g
s/\[go_u\]/${phonetic_go_u}/g
s/\[go_em\]/${phonetic_go_em}/g
s/\[io\]/${phonetic_io}/g
s/\[io_u\]/${phonetic_io_u}/g
s/\[io_em\]/${phonetic_io_em}/g
s/\[gu\]/${phonetic_gu}/g
s/\[gu_u\]/${phonetic_gu_u}/g
s/\[gu_em\]/${phonetic_gu_em}/g
s/\[iu\]/${phonetic_iu}/g
s/\[iu_u\]/${phonetic_iu_u}/g
s/\[iu_em\]/${phonetic_iu_em}/g
s/\[id\]/${phonetic_id}/g
s/\[id_u\]/${phonetic_id_u}/g
s/\[id_em\]/${phonetic_id_em}/g
s/\[iz\]/${phonetic_iz}/g
s/\[iz_u\]/${phonetic_iz_u}/g
s/\[iz_em\]/${phonetic_iz_em}/g
s/\[it\]/${phonetic_it}/g
s/\[it_u\]/${phonetic_it_u}/g
s/\[it_em\]/${phonetic_it_em}/g
/@notfound@/ { q 1 }
p
"

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
