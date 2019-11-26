#	$OpenBSD: agent-getpeereid.sh,v 1.11 2019/11/26 23:43:10 djm Exp $
#	Placed in the Public Domain.

tid="disallow agent attach from other uid"

UNPRIV=nobody
ASOCK=${OBJ}/agent
SSH_AUTH_SOCK=/nonexistent

case "x$SUDO" in
	xsudo) sudo=1;;
	xdoas) ;;
	x)
		if [ -x /usr/local/bin/sudo -a -f /etc/sudoers ]; then
			sudo=1
			SUDO=/usr/local/sbin/sudo
		elif [ -f /etc/doas.conf ]; then
			SUDO=/usr/bin/doas
		else
			echo neither sudo and sudoers nor doas.conf exist
			echo SKIPPED
			exit 0
		fi ;;
	*) fatal 'unsupported $SUDO - "doas" and "sudo" are allowed' ;;
esac

trace "start agent"
eval `${SSHAGENT} ${EXTRA_AGENT_ARGS} -s -a ${ASOCK}` > /dev/null
r=$?
if [ $r -ne 0 ]; then
	fail "could not start ssh-agent: exit code $r"
else
	chmod 644 ${SSH_AUTH_SOCK}

	${SSHADD} -l > /dev/null 2>&1
	r=$?
	if [ $r -ne 1 ]; then
		fail "ssh-add failed with $r != 1"
	fi
	if test -z "$sudo" ; then
		# doas
		${SUDO} -n -u ${UNPRIV} ${SSHADD} -l 2>/dev/null
	else
		# sudo
		< /dev/null ${SUDO} -S -u ${UNPRIV} ${SSHADD} -l 2>/dev/null
	fi
	r=$?
	if [ $r -lt 2 ]; then
		fail "ssh-add did not fail for ${UNPRIV}: $r < 2"
	fi

	trace "kill agent"
	${SSHAGENT} -k > /dev/null
fi

rm -f ${OBJ}/agent
