#	$OpenBSD: proxy-connect.sh,v 1.3 2002/02/16 01:09:47 markus Exp $
#	Placed in the Public Domain.

tid="proxy connect"

for p in 1 2; do
	ssh -$p -F $OBJ/ssh_proxy 999.999.999.999 true
	if [ $? -ne 0 ]; then
		fail "ssh proxyconnect protocol $p failed"
	fi
done
