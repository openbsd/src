#!/bin/sh
# $OpenBSD: skeyinfo.sh,v 1.4 1996/09/29 04:46:17 millert Exp $
# search /etc/skeykeys for the skey string for
# this user OR user specified in 1st parameter

KEYDB=/etc/skeykeys
if [ -z "$1" ]; then
	WHO=`/usr/bin/whoami`
else
	WHO=$1
fi

if [ -f $KEYDB ]; then
	/usr/bin/awk '/^'$WHO'[ 	]/ { if ($2 ~ /^[A-z]/) { print $3-1, $4} else { print $2-1, $3 } }' < $KEYDB
fi
