#	$Id: runlist.sh,v 1.2 1996/09/05 21:27:34 deraadt Exp $

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
