#!/bin/sh

# test if tail follows a file removed then recreated later

#set TMPDIR to a nfs-based dir for nfs testing
local DIR=$(mktemp -d)
echo DIR=${DIR}

local NAME=${0##*/}
local OUT=${DIR}/${NAME%%.sh}.out
local ERR=${DIR}/${NAME%%.sh}.err
echo bar > ${DIR}/bar

# retry until file appears for nfs
local RET=1
while [ ${RET} == 1 ] ; do
	tail -f ${DIR}/bar 2> ${ERR} > ${OUT} &
	RET=$?
	PID=$!
	sleep 1
done

# remove
rm -f ${DIR}/bar
echo 'bar2' > ${DIR}/bar2
sleep 1
mv ${DIR}/bar2 ${DIR}/bar
echo 'bar' >> ${DIR}/bar

# hey nfs !
sleep 5
kill ${PID}
diff -u ${OUT} ${0%%.sh}.out || exit 1
grep -q "tail: ${DIR}/bar has been replaced, reopening." ${ERR} || exit 2

# cleanup if okay
rm -Rf ${DIR}
