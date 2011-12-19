#!/bin/bash

PATH=/bin:/usr/bin

#
# This script converts CP949-encoded filenames into UTF-8 encoded filenames
#

# If unset, this script will rename the files without confirmation
PROMPT="yes"

# If unset, this script will not show hexdump of source filename
VERBOSE="yes"

for f in "$@"; do
    newname=`echo "$f" | iconv -f cp949 2>/dev/null`
    if test "$?" -eq 0; then
        if test "$f" != "$newname"; then
            echo "Source file: $f"
            if test -n "$VERBOSE"; then
                echo "$f" | hexdump -C
            fi

            echo "Destination file: $newname"
            if test -n "$PROMPT"; then
                read -e -n 1 -p " Rename? [y/n] " answer

                if test "$answer" = "y" -o "$answer" = "Y"; then
                    # do it.
                    mv -i "$f" "$newname"
                fi
            else
                # do it
                mv -i "$f" "$newname"
            fi
            echo ""
        fi
    fi
done
