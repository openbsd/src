tid="simple connect"

for p in 1 2; do
	ssh -o "Protocol=$p" -F $OBJ/ssh_config somehost true
	if [ $? -ne 0 ]; then
		fail "ssh connect with protocol $p failed"
	fi
done
