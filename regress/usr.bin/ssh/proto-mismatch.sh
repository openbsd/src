#	$OpenBSD: proto-mismatch.sh,v 1.2 2002/02/16 01:09:47 markus Exp $
#	Placed in the Public Domain.

tid="protocol version mismatch"

mismatch ()
{
	server=$1
	client=$2
	banner=`echo ${client} | sshd -o "Protocol=${server}" -i -f ${OBJ}/sshd_proxy`
	r=$?
	trace "sshd prints ${banner}"
	if [ $r -ne 255 ]; then
		fail "sshd prints ${banner} and accepts connect with version ${client}"
	fi
}

mismatch	2	SSH-1.5-HALLO
mismatch	1	SSH-2.0-HALLO
