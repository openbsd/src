#!/bin/sh

#
# Simple script to find perl and run it
#

IFS=:$IFS
perl=
for i in $PATH; do
    [ X"$i" = X ] && i=.
    for j in perl perl4 perl5 ; do
	[ -x "$i/$j" ] && perl=$i/$j && break 2
    done
done

[ X"$perl" = X ] && {
	echo "$0: can't find perl - bye\n" 1>&2
	exit 1
    }

exec $perl "$@"
