#	$OpenBSD: sftp-badcmds.sh,v 1.1 2003/04/04 09:34:22 djm Exp $
#	Placed in the Public Domain.

tid="sftp invalid commands"

DATA=/bin/ls
DATA2=/bin/cat
NONEXIST=/NONEXIST.$$
COPY=${OBJ}/copy

rm -rf ${COPY} ${COPY}.1 ${COPY}.2 ${COPY}.dd ${BATCH}.*

rm -f ${COPY}
verbose "$tid: get nonexistent"
echo "get $NONEXIST $COPY" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "get nonexistent failed"
test -f ${COPY} && fail "existing copy after get nonexistent"

rm -f ${COPY}
verbose "$tid: put nonexistent"
echo "put $NONEXIST $COPY" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "put nonexistent failed"
test -f ${COPY} && fail "existing copy after put nonexistent"

rm -f ${COPY}
verbose "$tid: rename nonexistent"
echo "rename $NONEXIST ${COPY}.1" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "rename nonexist failed"
test -f ${COPY}.1 && fail "file exists after rename nonexistent"

rm -f ${COPY} ${COPY}.1
cp $DATA $COPY
cp $DATA2 ${COPY}.1
verbose "$tid: rename target exists"
echo "rename $COPY ${COPY}.1" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "rename target exists failed"
test -f ${COPY} || fail "oldname missing after rename target exists"
test -f ${COPY}.1 || fail "newname missing after rename target exists"
cmp $DATA ${COPY} >/dev/null 2>&1 || fail "corrupted oldname after rename target exists"
cmp $DATA2 ${COPY}.1 >/dev/null 2>&1 || fail "corrupted newname after rename target exists"

rm -rf ${COPY} ${COPY}.dd
cp $DATA $COPY
mkdir ${COPY}.dd
verbose "$tid: rename target exists (directory)"
echo "rename $COPY ${COPY}.dd" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "rename target exists (directory) failed"
test -f ${COPY} || fail "oldname missing after rename target exists (directory)"
test -d ${COPY}.dd || fail "newname missing after rename target exists (directory)"
cmp $DATA ${COPY} >/dev/null 2>&1 || fail "corrupted oldname after rename target exists (directory)"

rm -rf ${COPY} ${COPY}.1 ${COPY}.2 ${COPY}.dd ${BATCH}.*


