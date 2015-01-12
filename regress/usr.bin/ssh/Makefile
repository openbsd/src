#	$OpenBSD: Makefile,v 1.73 2015/01/12 20:13:27 markus Exp $

REGRESS_FAIL_EARLY= yes
REGRESS_TARGETS=	unit t1 t2 t3 t4 t5 t6 t7 t8 t9 t10 t11 t12

CLEANFILES+=	t2.out t6.out1 t6.out2 t7.out t7.out.pub copy.1 copy.2 \
		t8.out t8.out.pub t9.out t9.out.pub t10.out t10.out.pub \
		t12.out t12.out.pub

LTESTS= 	connect \
		proxy-connect \
		connect-privsep \
		proto-version \
		proto-mismatch \
		exit-status \
		envpass \
		transfer \
		banner \
		rekey \
		dhgex \
		stderr-data \
		stderr-after-eof \
		broken-pipe \
		try-ciphers \
		yes-head \
		login-timeout \
		agent \
		agent-getpeereid \
		agent-timeout \
		agent-ptrace \
		keyscan \
		keygen-change \
		keygen-convert \
		key-options \
		scp \
		sftp \
		sftp-chroot \
		sftp-cmds \
		sftp-badcmds \
		sftp-batch \
		sftp-glob \
		sftp-perm \
		reconfigure \
		dynamic-forward \
		forwarding \
		multiplex \
		reexec \
		brokenkeys \
		cfgmatch \
		addrmatch \
		localcommand \
		forcecommand \
		portnum \
		keytype \
		kextype \
		cert-hostkey \
		cert-userkey \
		host-expand \
		keys-command \
		forward-control \
		integrity \
		krl \
		multipubkey

INTEROP_TESTS=	putty-transfer putty-ciphers putty-kex conch-ciphers
#INTEROP_TESTS+=ssh-com ssh-com-client ssh-com-keygen ssh-com-sftp

#LTESTS= 	cipher-speed

USER!=		id -un
CLEANFILES+=	authorized_keys_${USER} known_hosts pidfile \
		ssh_config sshd_config.orig ssh_proxy sshd_config sshd_proxy \
		rsa.pub rsa rsa1.pub rsa1 host.rsa host.rsa1 \
		rsa-agent rsa-agent.pub rsa1-agent rsa1-agent.pub \
		ls.copy banner.in banner.out empty.in \
		scp-ssh-wrapper.exe ssh_proxy_envpass remote_pid \
		sshd_proxy_bak rsa_ssh2_cr.prv rsa_ssh2_crnl.prv \
		known_hosts-cert host_ca_key* cert_user_key* cert_host_key* \
		authorized_principals_${USER} expect actual ready \
		sshd_proxy.* authorized_keys_${USER}.* revoked-* krl-* \
		ssh.log failed-ssh.log sshd.log failed-sshd.log \
		regress.log failed-regress.log ssh-log-wrapper.sh \
		sftp-server.sh sftp-server.log sftp.log

SUDO_CLEAN+=	/var/run/testdata_${USER} /var/run/keycommand_${USER}

# Enable all malloc(3) randomisations and checks
TEST_ENV=      "MALLOC_OPTIONS=AFGJPRX"

unit:
	if test -z "${SKIP_UNIT}" ; then \
		(set -e ; cd ${.CURDIR}/unittests ; make) \
	fi

t1:
	ssh-keygen -if ${.CURDIR}/rsa_ssh2.prv | diff - ${.CURDIR}/rsa_openssh.prv
	tr '\n' '\r' <${.CURDIR}/rsa_ssh2.prv > ${.OBJDIR}/rsa_ssh2_cr.prv
	ssh-keygen -if ${.OBJDIR}/rsa_ssh2_cr.prv | diff - ${.CURDIR}/rsa_openssh.prv
	awk '{print $$0 "\r"}' ${.CURDIR}/rsa_ssh2.prv > ${.OBJDIR}/rsa_ssh2_crnl.prv
	ssh-keygen -if ${.OBJDIR}/rsa_ssh2_crnl.prv | diff - ${.CURDIR}/rsa_openssh.prv

t2:
	cat ${.CURDIR}/rsa_openssh.prv > t2.out
	chmod 600 t2.out
	ssh-keygen -yf t2.out | diff - ${.CURDIR}/rsa_openssh.pub

t3:
	ssh-keygen -ef ${.CURDIR}/rsa_openssh.pub |\
		ssh-keygen -if /dev/stdin |\
		diff - ${.CURDIR}/rsa_openssh.pub

t4:
	ssh-keygen -E md5 -lf ${.CURDIR}/rsa_openssh.pub |\
		awk '{print $$2}' | diff - ${.CURDIR}/t4.ok

t5:
	ssh-keygen -Bf ${.CURDIR}/rsa_openssh.pub |\
		awk '{print $$2}' | diff - ${.CURDIR}/t5.ok

t6:
	ssh-keygen -if ${.CURDIR}/dsa_ssh2.prv > t6.out1
	ssh-keygen -if ${.CURDIR}/dsa_ssh2.pub > t6.out2
	chmod 600 t6.out1
	ssh-keygen -yf t6.out1 | diff - t6.out2

t7.out:
	ssh-keygen -q -t rsa -N '' -f $@

t7: t7.out
	ssh-keygen -lf t7.out > /dev/null
	ssh-keygen -Bf t7.out > /dev/null

t8.out:
	ssh-keygen -q -t dsa -N '' -f $@

t8: t8.out
	ssh-keygen -lf t8.out > /dev/null
	ssh-keygen -Bf t8.out > /dev/null

t9.out:
	ssh-keygen -q -t ecdsa -N '' -f $@

t9: t9.out
	ssh-keygen -lf t9.out > /dev/null
	ssh-keygen -Bf t9.out > /dev/null

t10.out:
	ssh-keygen -q -t ed25519 -N '' -f $@

t10: t10.out
	ssh-keygen -lf t10.out > /dev/null
	ssh-keygen -Bf t10.out > /dev/null

t11:
	ssh-keygen -E sha256 -lf ${.CURDIR}/rsa_openssh.pub |\
		awk '{print $$2}' | diff - ${.CURDIR}/t11.ok

t12.out:
	ssh-keygen -q -t ed25519 -N '' -C 'test-comment-1234' -f $@

t12: t12.out
	ssh-keygen -lf t12.out.pub | grep -q test-comment-1234

modpipe: modpipe.c

t-integrity: modpipe

.for t in ${LTESTS} ${INTEROP_TESTS}
t-${t}:
	env SUDO="${SUDO}" ${TEST_ENV} \
	    sh ${.CURDIR}/test-exec.sh ${.OBJDIR} ${.CURDIR}/${t}.sh
.endfor

.for t in ${LTESTS}
REGRESS_TARGETS+=t-${t}
.endfor

.for t in ${INTEROP_TESTS}
INTEROP_TARGETS+=t-${t}
.endfor

# Not run by default
interop: ${INTEROP_TARGETS}

clean:
	rm -f ${CLEANFILES}
	test -z "${SUDO}" || ${SUDO} rm -f ${SUDO_CLEAN}
	rm -rf .putty
	(set -e ; cd ${.CURDIR}/unittests ; make clean)

.include <bsd.regress.mk>
