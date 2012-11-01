#!/bin/sh

# test if tail follows a file descriptor across renames

#set TMPDIR to a nfs-based dir for nfs testing
local DIR=$(mktemp -d)
echo DIR=${DIR}

local NAME=${0##*/}
local OUT=${DIR}/${NAME%%.sh}.out
echo bar > ${DIR}/bar

# retry until file appears for nfs
local RET=1
while [ ${RET} == 1 ] ; do
	tail -f ${DIR}/bar > ${OUT} &
	RET=$?
	PID=$!
	sleep 1
done

mv ${DIR}/bar ${DIR}/bar2
echo 'bar2' >> ${DIR}/bar2
mv ${DIR}/bar2 ${DIR}/bar
echo 'bar' >> ${DIR}/bar

# hey nfs !
sleep 5
kill ${PID}
diff -u ${OUT} ${0%%.sh}.out || exit 1

# cleanup if okay
rm -Rf ${DIR}
