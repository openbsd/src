#!/bin/sh
#
# $OpenBSD: follow-newsyslog.sh,v 1.2 2012/11/03 08:41:25 ajacoutot Exp $

# test if tail follows a file rotated by newsyslog

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

echo "${DIR}/bar 644 1 1 *" > ${DIR}/newsyslog.conf
newsyslog -Ff ${DIR}/newsyslog.conf
echo 'bar' >> ${DIR}/bar
sleep 1
echo "${DIR}/bar 644 0 1 *" > ${DIR}/newsyslog.conf
newsyslog -Ff ${DIR}/newsyslog.conf
echo 'bar' >> ${DIR}/bar

# hey nfs !
sleep 5
kill ${PID}
# no diff this time, output too complex
[ $(grep -c -e '^bar$' ${OUT}) -eq 3 ] || exit 1
[ $(grep -c -e 'newsyslog\[.*\]: logfile turned over' ${OUT}) -eq 4 ] || exit 2
tail -1 ${OUT} | grep -q -e '^bar$' || exit 3
head -1 ${OUT} | grep -q -e '^bar$' || exit 4

#[ $(grep -c "tail: ${DIR}/bar has been truncated, resetting." ${ERR}) -eq 2 ] || exit $?
[ $(grep -c "tail: ${DIR}/bar has been replaced, reopening." ${ERR}) -eq 2 ] || exit 5

# cleanup if okay
rm -Rf ${DIR}
