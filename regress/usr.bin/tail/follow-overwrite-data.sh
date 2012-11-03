#!/bin/sh
#
# $OpenBSD: follow-overwrite-data.sh,v 1.2 2012/11/03 08:41:25 ajacoutot Exp $

# test if tail follows a file overwritten by data

#set TMPDIR to a nfs-based dir for nfs testing
DIR=$(mktemp -d)
echo DIR=${DIR}

NAME=${0##*/}
OUT=${DIR}/${NAME%%.sh}.out
ERR=${DIR}/${NAME%%.sh}.err
echo bar > ${DIR}/bar

# retry until file appears for nfs
RET=1
while [ ${RET} == 1 ] ; do
	tail -f ${DIR}/bar 2> ${ERR} > ${OUT} &
	RET=$?
	PID=$!
	sleep 1
done

# smaller data
echo 'ba' > ${DIR}/bar
sleep 1
# bigger data with delay
echo 'baar' > ${DIR}/bar
# smaller data without delay
echo 'bar' > ${DIR}/bar

# hey nfs !
sleep 5
kill ${PID}
diff -u ${OUT} ${0%%.sh}.out || exit 1
[ $(grep -c "tail: ${DIR}/bar has been truncated, resetting." ${ERR}) -eq 3 ] || exit 2

# cleanup if okay
rm -Rf ${DIR}
