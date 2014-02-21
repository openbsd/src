#	$OpenBSD: runlist.sh,v 1.5 2014/02/21 16:30:14 deraadt Exp $
#	$NetBSD: runlist.sh,v 1.1 1995/12/18 22:47:38 pk Exp $

if [ "X$1" = "X-d" ]; then
	SHELLCMD=cat
	shift
else
	SHELLCMD="sh -e"
fi

( while [ "X$1" != "X" ]; do
	cat $1
	shift
done ) | awk -f ${UTILS:-${CURDIR}}/list2sh.awk | ${SHELLCMD}
