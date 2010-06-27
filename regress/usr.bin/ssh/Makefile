#	$OpenBSD: Makefile,v 1.54 2010/06/27 19:19:56 phessler Exp $

REGRESS_TARGETS=	t1 t2 t3 t4 t5 t6 t7

CLEANFILES+=	t2.out t6.out1 t6.out2 t7.out t7.out.pub copy.1 copy.2

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
		sftp-cmds \
		sftp-badcmds \
		sftp-batch \
		sftp-glob \
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
		cert-hostkey \
		cert-userkey

INTEROP_TESTS=	putty-transfer putty-ciphers putty-kex conch-ciphers
#INTEROP_TESTS+=ssh-com ssh-com-client ssh-com-keygen ssh-com-sftp


USER!=		id -un
CLEANFILES+=	authorized_keys_${USER} known_hosts pidfile \
		ssh_config sshd_config.orig ssh_proxy sshd_config sshd_proxy \
		rsa.pub rsa rsa1.pub rsa1 host.rsa host.rsa1 \
		rsa-agent rsa-agent.pub rsa1-agent rsa1-agent.pub \
		ls.copy banner.in banner.out empty.in \
		scp-ssh-wrapper.exe ssh_proxy_envpass remote_pid \
		sshd_proxy_bak rsa_ssh2_cr.prv rsa_ssh2_crnl.prv \
		known_hosts-cert host_ca_key* cert_host_key* \
		authorized_principals_${USER}

# Enable all malloc(3) randomisations and checks
TEST_ENV=      "MALLOC_OPTIONS=AFGJPRX"

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
	ssh-keygen -lf ${.CURDIR}/rsa_openssh.pub |\
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
	rm -rf .putty

.include <bsd.regress.mk>
