#!/bin/ksh
# $OpenBSD: id.sh,v 1.1 2004/01/15 17:14:43 sturm Exp $

echo "/bin/systrace -f $1 -a /usr/bin/id"
SYSTR_RES=`eval /bin/systrace -f $1 -a /usr/bin/id 2>/dev/null`
NORM_RES=`/usr/bin/id`
if [ -z "$SYSTR_RES" ] ; then
	rm -f id.core
	echo "Systrace of /usr/bin/id failed"
	exit 1
fi

if [ "$NORM_RES" != "$SYSTR_RES" ] ; then
	echo "Expected \"$NORM_RES\""
	echo "Got \"$SYSTR_RES\""
	exit 1
fi

exit 0
