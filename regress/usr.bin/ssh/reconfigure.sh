#	$OpenBSD: reconfigure.sh,v 1.5 2015/03/03 22:35:19 markus Exp $
#	Placed in the Public Domain.

tid="simple connect after reconfigure"

# we need the full path to sshd for -HUP
start_sshd

trace "connect before restart"
for p in ${SSH_PROTOCOLS} ; do
	${SSH} -o "Protocol=$p" -F $OBJ/ssh_config somehost true
	if [ $? -ne 0 ]; then
		fail "ssh connect with protocol $p failed before reconfigure"
	fi
done

$SUDO kill -HUP `cat $PIDFILE`
sleep 1

trace "wait for sshd to restart"
i=0;
while [ ! -f $PIDFILE -a $i -lt 10 ]; do
	i=`expr $i + 1`
	sleep $i
done

test -f $PIDFILE || fatal "sshd did not restart"

trace "connect after restart"
for p in ${SSH_PROTOCOLS} ; do
	${SSH} -o "Protocol=$p" -F $OBJ/ssh_config somehost true
	if [ $? -ne 0 ]; then
		fail "ssh connect with protocol $p failed after reconfigure"
	fi
done
