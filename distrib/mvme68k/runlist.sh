#	$OpenBSD: runlist.sh,v 1.3 2000/03/01 22:10:03 todd Exp $
#	$NetBSD: runlist.sh,v 1.1 1995/07/18 04:13:01 briggs Exp $

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
