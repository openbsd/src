tid="proxy connect"

for p in 1 2; do
	ssh -$p -F $OBJ/ssh_proxy 999.999.999.999 true
	if [ $? -ne 0 ]; then
		fail "ssh proxyconnect protocol $p failed"
	fi
done
