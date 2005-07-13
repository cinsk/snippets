#!/bin/sh
# -*-sh-mode-*-

#
# Skeleton Shell script using getopt(1)
# Copyright (C) 2005  Seong-Kook Shin <cinsky at gmail dot com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

# $Id$

PATH=/usr/local/bin:/usr/bin:/bin

program_name='skel2'
version_string='0.1'

output_filename=
verbose_mode=1

function error() {
    # usage: error EXIT_STATUS MESSAGE...
    # Print MESSAGE to the standard error (stderr) and call `exit EXIT_STATUS'
    # if EXIT-STATUS is nonzero.
    status="$1"
    shift;
    test -n "$program_name" && echo -n "$program_name: "
    echo "$@"
    test 0 -ne "$status" && exit "$status";
} 1>&2

function help_and_exit() {
    cat <<EOF
usage: $program_name [OPTION...] FILE...

  -q, --quiet        quite mode
  -o, --output=FILE  send output to file FILE. If FILE is \`-',
                     send output to stdout.

      --help         display this help and exit
      --version      output version information and exit

Report bugs to <cinsky at gmail dot com>

EOF
    exit 0
}

function version_and_exit() {
    cat <<EOF
$program_name version $version_string
EOF
    exit 0
}


# The actual code begins here -------------

GETOPT=`getopt -o o:q --long help,version,quiet,output: -n "$program_name" -- "$@"`
if [ $? -ne 0 ]; then
    error 1 "Try \`$program_name --help' for more information"
fi

eval set -- "$GETOPT"

while true; do
    case "$1" in
        --help)
            help_and_exit
            shift
            ;;
        --version)
            version_and_exit
            shift
            ;;
        -o|--output)
            output_filename="$2"
            shift 2
            ;;
        -q|--quiet)
            verbose_mode=0
            shift;
            ;;
        --)
            shift
            break
            ;;
    esac
done

echo "output_filename: $output_filename"
echo "verbose_mode: $verbose_mode"

for f in "$@"; do
    echo "arg = $f"
done
