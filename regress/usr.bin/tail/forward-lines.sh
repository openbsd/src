#!/bin/sh
#
# $OpenBSD: forward-lines.sh,v 1.1 2015/11/24 04:04:19 tedu Exp $

# test if tail grep the correct number of blocks from a file.

DIR=$(mktemp -d)
echo DIR=${DIR}

NAME=${0##*/}
OUT=${DIR}/${NAME%%.sh}.out
i=0
while [ ${i} -lt 512 ]; do
	echo ${i} >> ${DIR}/bar
	i=$((i+1))
done

tail -n +5 ${DIR}/bar > ${OUT}
tail +5 ${DIR}/bar > ${OUT}1

diff -u ${OUT} ${OUT}1 || exit 1
diff -u ${OUT} ${0%%.sh}.out || exit 1

# cleanup if okay
rm -Rf ${DIR}
