#	$OpenBSD: connect.sh,v 1.3 2002/02/16 01:09:47 markus Exp $
#	Placed in the Public Domain.

tid="simple connect"

start_sshd

for p in 1 2; do
	ssh -o "Protocol=$p" -F $OBJ/ssh_config somehost true
	if [ $? -ne 0 ]; then
		fail "ssh connect with protocol $p failed"
	fi
done
