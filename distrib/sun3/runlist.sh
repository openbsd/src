#	$OpenBSD: runlist.sh,v 1.1 1998/08/23 18:08:56 kstailey Exp $

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
