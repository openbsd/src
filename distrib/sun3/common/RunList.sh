#!/bin/sh

#	$OpenBSD: RunList.sh,v 1.2 2000/03/01 22:10:09 todd Exp $
#	$NetBSD: RunList.sh,v 1.1.1.1 1995/10/08 23:07:47 gwr Exp $

if [ "X$1" = "X-d" ]; then
	SHELLCMD=cat
	shift
else
	SHELLCMD="sh -e"
fi

cat "$@" |
awk -f ${TOPDIR}/common/RunList.awk |
${SHELLCMD}
