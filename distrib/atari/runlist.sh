#	$OpenBSD: runlist.sh,v 1.2 2000/03/01 22:10:00 todd Exp $
#	$NetBSD: runlist.sh,v 1.1.1.1 1995/04/17 19:08:49 leo Exp $

if [ "X$1" = "X-d" ]; then
	SHELLCMD=cat
	shift
else
	SHELLCMD="sh -e"
fi

( while [ "X$1" != "X" ]; do
	cat $1
	shift
done ) | awk -f ${TOPDIR}/list2sh.awk | ${SHELLCMD}
