#	$OpenBSD: reexec.sh,v 1.1 2004/06/24 19:32:00 djm Exp $
#	Placed in the Public Domain.

tid="reexec tests"

DATA=/bin/ls
COPY=${OBJ}/copy

verbose "test config passing"
cp $OBJ/sshd_config $OBJ/sshd_config.orig

start_sshd

echo "InvalidXXX=no" >> $OBJ/sshd_config

rm -f ${COPY}
for p in 1 2; do
	verbose "$tid: proto $p"
	${SSH} -nqo "Protocol=$p" -F $OBJ/ssh_config somehost \
	    cat ${DATA} > ${COPY}
	if [ $? -ne 0 ]; then
		fail "ssh cat $DATA failed"
	fi
	cmp ${DATA} ${COPY}		|| fail "corrupted copy"
	rm -f ${COPY}
done

$SUDO kill `cat $PIDFILE`
rm -f $PIDFILE

cp $OBJ/sshd_config.orig $OBJ/sshd_config

verbose "test reexec fallback"

start_sshd_copy_zap

rm -f ${COPY}
for p in 1 2; do
	verbose "$tid: proto $p"
	${SSH} -nqo "Protocol=$p" -F $OBJ/ssh_config somehost \
	    cat ${DATA} > ${COPY}
	if [ $? -ne 0 ]; then
		fail "ssh cat $DATA failed"
	fi
	cmp ${DATA} ${COPY}		|| fail "corrupted copy"
	rm -f ${COPY}
done

$SUDO kill `cat $PIDFILE`
rm -f $PIDFILE

verbose "test reexec fallback without privsep"

cp $OBJ/sshd_config.orig $OBJ/sshd_config
echo "UsePrivilegeSeparation=no" >> $OBJ/sshd_config

start_sshd_copy_zap

rm -f ${COPY}
for p in 1 2; do
	verbose "$tid: proto $p"
	${SSH} -nqo "Protocol=$p" -F $OBJ/ssh_config somehost \
	    cat ${DATA} > ${COPY}
	if [ $? -ne 0 ]; then
		fail "ssh cat $DATA failed"
	fi
	cmp ${DATA} ${COPY}		|| fail "corrupted copy"
	rm -f ${COPY}
done

$SUDO kill `cat $PIDFILE`
rm -f $PIDFILE

cp $OBJ/sshd_config.orig $OBJ/sshd_config

