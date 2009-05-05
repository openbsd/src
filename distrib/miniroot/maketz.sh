#!/bin/sh

destdir=$1

if [ $# -lt 1 ]; then
	echo usage: maketz.sh DESTDIR
	exit 0
fi
echo maketz args $destdir

#ls -1dF `find . -print` | \
#   sed -e 's/^.\///' | grep -v '^zone.tab$' | grep -v '^$') > var/tzlist

mkdir -p var/tzdir
cd var/tzdir

touch FOO
(cd $destdir/usr/share/zoneinfo; find . -type d -print0) | xargs -0 mkdir -p
(cd $destdir/usr/share/zoneinfo; find . -type f -print0) | xargs -0 -n 1 ln FOO
rm FOO
