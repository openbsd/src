#	$OpenBSD: agent-getpeereid.sh,v 1.14 2022/12/01 02:19:29 dtucker Exp $
#	Placed in the Public Domain.

tid="disallow agent attach from other uid"

UNPRIV=nobody
ASOCK=${OBJ}/agent
SSH_AUTH_SOCK=/nonexistent
>$OBJ/ssh-agent.log
>$OBJ/ssh-add.log

case "x$SUDO" in
	xsudo) sudo=1;;
	xdoas|xdoas\ *) ;;
	x)
		if [ -x /usr/local/bin/sudo -a -f /etc/sudoers ]; then
			sudo=1
			SUDO=/usr/local/sbin/sudo
		elif [ -f /etc/doas.conf ]; then
			SUDO=/usr/bin/doas
		else
			skip "neither sudo and sudoers nor doas.conf exist"
		fi ;;
	*) fatal 'unsupported $SUDO - "doas" and "sudo" are allowed' ;;
esac

trace "start agent"
eval `${SSHAGENT} ${EXTRA_AGENT_ARGS} -s -a ${ASOCK}` >$OBJ/ssh-agent.log 2>&1
r=$?
if [ $r -ne 0 ]; then
	fail "could not start ssh-agent: exit code $r"
else
	chmod 644 ${SSH_AUTH_SOCK}

	${SSHADD} -vvv -l >>$OBJ/ssh-add.log 2>&1
	r=$?
	if [ $r -ne 1 ]; then
		fail "ssh-add failed with $r != 1"
	fi
	if test -z "$sudo" ; then
		# doas
		${SUDO} -n -u ${UNPRIV} ${SSHADD} -vvv -l >>$OBJ/ssh-add.log 2>&1
	else
		# sudo
		< /dev/null ${SUDO} -S -u ${UNPRIV} ${SSHADD} -vvv -l >>$OBJ/ssh-add.log 2>&1
	fi
	r=$?
	if [ $r -lt 2 ]; then
		fail "ssh-add did not fail for ${UNPRIV}: $r < 2"
		cat $OBJ/ssh-add.log
	fi

	trace "kill agent"
	${SSHAGENT} -vvv -k >>$OBJ/ssh-agent.log 2>&1
fi

rm -f ${OBJ}/agent
