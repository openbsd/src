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
