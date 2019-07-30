#!/bin/sh

destdir=$1

if [ $# -lt 1 ]; then
	echo usage: maketz.sh DESTDIR
	exit 0
fi

(
	cd $destdir/usr/share/zoneinfo
	ls -1dF `tar cvf /dev/null [A-Za-y]*`
) > var/tzlist
