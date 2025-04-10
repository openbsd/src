#	$OpenBSD: dropbear-kex.sh,v 1.4 2025/03/11 07:42:08 dtucker Exp $
#	Placed in the Public Domain.

tid="dropbear kex"

if test "x$REGRESS_INTEROP_DROPBEAR" != "xyes" ; then
	skip "dropbear interop tests not enabled"
fi

cp $OBJ/sshd_proxy $OBJ/sshd_proxy.bak

kex="curve25519-sha256 curve25519-sha256@libssh.org"
if $SSH -Q kex | grep 'diffie-hellman-group14-sha256' >/dev/null; then
	kex="$kex diffie-hellman-group14-sha256"
fi
# There's no flag to query KEX, so if MACs does not contain SHA1, assume
# there's also SHA1-based KEX methods either.
if $SSH -Q kex | grep 'diffie-hellman-group14-sha1' >/dev/null && \
    $DBCLIENT -m help hst 2>&1 | grep -- '-sha1' >/dev/null ; then
	kex="$kex diffie-hellman-group14-sha1"
fi

for k in $kex; do
	verbose "$tid: kex $k"
	rm -f ${COPY}
	# dbclient doesn't have switch for kex, so force in server
	(cat $OBJ/sshd_proxy.bak; echo "KexAlgorithms $k") >$OBJ/sshd_proxy
	env HOME=$OBJ \
	    ${DBCLIENT} -y -i $OBJ/.dropbear/id_ed25519 2>$OBJ/dbclient.log \
	   -J "$OBJ/ssh_proxy.sh" somehost cat ${DATA} > ${COPY}
	if [ $? -ne 0 ]; then
		fail "ssh cat $DATA failed"
	fi
	cmp ${DATA} ${COPY}		|| fail "corrupted copy"
done
rm -f ${COPY}
