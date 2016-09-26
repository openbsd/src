#	$OpenBSD: agent-getpeereid.sh,v 1.7 2016/09/26 21:34:38 bluhm Exp $
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
			fatal "need SUDO to switch to uid $UNPRIV," \
			    "test won't work without"
		fi ;;
	*) fatal 'unsupported $SUDO - "doas" and "sudo" are allowed' ;;
esac

trace "start agent"
eval `${SSHAGENT} -s -a ${ASOCK}` > /dev/null
r=$?
if [ $r -ne 0 ]; then
	fail "could not start ssh-agent: exit code $r"
else
	chmod 644 ${SSH_AUTH_SOCK}

	ssh-add -l > /dev/null 2>&1
	r=$?
	if [ $r -ne 1 ]; then
		fail "ssh-add failed with $r != 1"
	fi
	if test -z "$sudo" ; then
		# doas
		${SUDO} -n -u ${UNPRIV} ssh-add -l 2>/dev/null
	else
		# sudo
		< /dev/null ${SUDO} -S -u ${UNPRIV} ssh-add -l 2>/dev/null
	fi
	r=$?
	if [ $r -lt 2 ]; then
		fail "ssh-add did not fail for ${UNPRIV}: $r < 2"
	fi

	trace "kill agent"
	${SSHAGENT} -k > /dev/null
fi

rm -f ${OBJ}/agent
