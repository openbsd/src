tid="try ciphers"

ciphers="aes128-cbc 3des-cbc blowfish-cbc cast128-cbc arcfour aes192-cbc aes256-cbc"
macs="hmac-sha1 hmac-md5 hmac-sha1-96 hmac-md5-96"

for c in $ciphers; do
	for m in $macs; do
		trace "proto 2 mac $m cipher $c"
		ssh -F $OBJ/ssh_proxy -2 -m $m -c $c somehost true
		if [ $? -ne 0 ]; then
			fail "ssh -2 failed with mac $m cipher $c"
		fi
	done
done

ciphers="3des blowfish"
for c in $ciphers; do
	trace "proto 1 cipher $c"
	ssh -F $OBJ/ssh_proxy -1 -c $c somehost true
	if [ $? -ne 0 ]; then
		fail "ssh -1 failed with cipher $c"
	fi
done
