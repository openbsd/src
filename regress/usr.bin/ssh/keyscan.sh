#	$OpenBSD: keyscan.sh,v 1.2 2002/02/22 12:38:27 markus Exp $
#	Placed in the Public Domain.

tid="keyscan"

# remove DSA hostkey
rm -f ${OBJ}/host.dsa

start_sshd

for t in rsa1 rsa dsa; do
	trace "keyscan type $t"
	ssh-keyscan -t $t -p $PORT 127.0.0.1 127.0.0.1 127.0.0.1 \
		> /dev/null 2>&1
	r=$?
	if [ $r -ne 0 ]; then
		fail "ssh-keyscan -t $t failed with: $r"
	fi
done
