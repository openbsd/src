#	$OpenBSD: runlist.sh,v 1.2 1999/06/13 10:33:30 downsj Exp $
#	$NetBSD: runlist.sh,v 1.1 1995/10/03 22:47:57 thorpej Exp $

if [ "X$1" = "X-d" ]; then
	SHELLCMD=cat
	shift
else
	SHELLCMD="sh"
fi

( while [ "X$1" != "X" ]; do
	cat $1
	shift
done ) | awk -f ${TOPDIR}/list2sh.awk | ${SHELLCMD}
