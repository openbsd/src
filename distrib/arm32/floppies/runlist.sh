# $OpenBSD: runlist.sh,v 1.2 2000/03/01 22:09:58 todd Exp $
# $NetBSD: runlist.sh,v 1.1 1996/05/16 19:58:52 mark Exp $

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
