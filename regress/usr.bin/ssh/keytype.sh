#	$OpenBSD: keytype.sh,v 1.4 2015/07/10 06:23:25 markus Exp $
#	Placed in the Public Domain.

tid="login with different key types"

TIME=/usr/bin/time
cp $OBJ/sshd_proxy $OBJ/sshd_proxy_bak
cp $OBJ/ssh_proxy $OBJ/ssh_proxy_bak

ktypes="dsa-1024 rsa-2048 ecdsa-256 rsa-3072 ecdsa-384 ecdsa-521 ed25519-512"

for kt in $ktypes; do 
	rm -f $OBJ/key.$kt
	bits=${kt#*-}
	type=${kt%-*} 
	printf "keygen $type, $bits bits:\t"
	${TIME} ${SSHKEYGEN} -b $bits -q -N '' -t $type  -f $OBJ/key.$kt ||\
		fail "ssh-keygen for type $type, $bits bits failed"
done

tries="1 2 3"
for ut in $ktypes; do 
	htypes=$ut
	#htypes=$ktypes
	for ht in $htypes; do 
		case $ht in
		dsa-1024)	t=ssh-dss;;
		ecdsa-256)	t=ecdsa-sha2-nistp256;;
		ecdsa-384)	t=ecdsa-sha2-nistp384;;
		ecdsa-521)	t=ecdsa-sha2-nistp521;;
		ed25519-512)	t=ssh-ed25519;;
		rsa-*)		t=ssh-rsa;;
		esac
		trace "ssh connect, userkey $ut, hostkey $ht"
		(
			grep -v HostKey $OBJ/sshd_proxy_bak
			echo HostKey $OBJ/key.$ht 
			echo PubkeyAcceptedKeyTypes $t
			echo HostKeyAlgorithms $t
		) > $OBJ/sshd_proxy
		(
			grep -v IdentityFile $OBJ/ssh_proxy_bak
			echo IdentityFile $OBJ/key.$ut 
			echo PubkeyAcceptedKeyTypes $t
			echo HostKeyAlgorithms $t
		) > $OBJ/ssh_proxy
		(
			printf 'localhost-with-alias,127.0.0.1,::1 '
			cat $OBJ/key.$ht.pub
		) > $OBJ/known_hosts
		cat $OBJ/key.$ut.pub > $OBJ/authorized_keys_$USER
		for i in $tries; do
			printf "userkey $ut, hostkey ${ht}:\t"
			${TIME} ${SSH} -F $OBJ/ssh_proxy 999.999.999.999 true
			if [ $? -ne 0 ]; then
				fail "ssh userkey $ut, hostkey $ht failed"
			fi
		done
	done
done
