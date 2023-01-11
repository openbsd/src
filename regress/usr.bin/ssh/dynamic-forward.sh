#	$OpenBSD: dynamic-forward.sh,v 1.16 2023/01/11 00:51:27 djm Exp $
#	Placed in the Public Domain.

tid="dynamic forwarding"

FWDPORT=`expr $PORT + 1`
CTL=$OBJ/ctl-sock
cp $OBJ/ssh_config $OBJ/ssh_config.orig
proxycmd="nc -x 127.0.0.1:$FWDPORT -X"
trace "will use ProxyCommand $proxycmd"

start_ssh() {
	direction="$1"
	arg="$2"
	n=0
	error="1"
	# Use a multiplexed ssh so we can control its lifecycle.
	trace "start dynamic -$direction forwarding, fork to background"
	(cat $OBJ/ssh_config.orig ; echo "$arg") > $OBJ/ssh_config
	${REAL_SSH} -vvvnNfF $OBJ/ssh_config -E$TEST_SSH_LOGFILE \
	    -$direction $FWDPORT -oExitOnForwardFailure=yes \
	    -oControlMaster=yes -oControlPath=$CTL somehost
	r=$?
	test $r -eq 0 || fatal "failed to start dynamic forwarding $r"
	if ! ${REAL_SSH} -qF$OBJ/ssh_config -O check \
	     -oControlPath=$CTL somehost >/dev/null 2>&1 ; then
		fatal "forwarding ssh process unresponsive"
	fi
}

stop_ssh() {
	test -S $CTL || return
	if ! ${REAL_SSH} -qF$OBJ/ssh_config -O exit \
	     -oControlPath=$CTL >/dev/null somehost >/dev/null ; then
		fatal "forwarding ssh process did not respond to close"
	fi
	n=0
	while [ "$n" -lt 20 ] ; do
		test -S $CTL || break
		sleep 1
		n=`expr $n + 1`
	done
	if test -S $CTL ; then
		fatal "forwarding ssh process did not exit"
	fi
}

check_socks() {
	direction=$1
	expect_success=$2
	for s in 4 5; do
	    for h in 127.0.0.1 localhost; do
		trace "testing ssh socks version $s host $h (-$direction)"
		${REAL_SSH} -q -F $OBJ/ssh_config \
			-o "ProxyCommand ${proxycmd}${s} $h $PORT 2>/dev/null" \
			somehost cat ${DATA} > ${COPY}
		r=$?
		if [ "x$expect_success" = "xY" ] ; then
			if [ $r -ne 0 ] ; then
				fail "ssh failed with exit status $r"
			fi
			test -f ${COPY}	 || fail "failed copy ${DATA}"
			cmp ${DATA} ${COPY} || fail "corrupted copy of ${DATA}"
		elif [ $r -eq 0 ] ; then
			fail "ssh unexpectedly succeeded"
		fi
	    done
	done
}

start_sshd
trap "stop_ssh" EXIT

for d in D R; do
	verbose "test -$d forwarding"
	start_ssh $d
	check_socks $d Y
	stop_ssh
	test "x$d" = "xR" || continue
	
	# Test PermitRemoteOpen
	verbose "PermitRemoteOpen=any"
	start_ssh $d PermitRemoteOpen=any
	check_socks $d Y
	stop_ssh

	verbose "PermitRemoteOpen=none"
	start_ssh $d PermitRemoteOpen=none
	check_socks $d N
	stop_ssh

	verbose "PermitRemoteOpen=explicit"
	start_ssh $d \
	    PermitRemoteOpen="127.0.0.1:$PORT [::1]:$PORT localhost:$PORT"
	check_socks $d Y
	stop_ssh

	verbose "PermitRemoteOpen=disallowed"
	start_ssh $d \
	    PermitRemoteOpen="127.0.0.1:1 [::1]:1 localhost:1"
	check_socks $d N
	stop_ssh
done
