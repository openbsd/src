#	$OpenBSD: Makefile,v 1.33 2004/10/29 23:59:22 djm Exp $

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
		scp \
		sftp \
		sftp-cmds \
		sftp-badcmds \
		sftp-batch \
		reconfigure \
		dynamic-forward \
		forwarding \
		multiplex \
		reexec \
		brokenkeys

USER!=		id -un
CLEANFILES+=	authorized_keys_${USER} known_hosts pidfile \
		ssh_config sshd_config.orig ssh_proxy sshd_config sshd_proxy \
		rsa.pub rsa rsa1.pub rsa1 host.rsa host.rsa1 \
		rsa-agent rsa-agent.pub rsa1-agent rsa1-agent.pub \
		ls.copy banner.in banner.out empty.in \
		scp-ssh-wrapper.exe

#LTESTS+=	ssh-com ssh-com-client ssh-com-keygen ssh-com-sftp

t1:
	ssh-keygen -if ${.CURDIR}/rsa_ssh2.prv | diff - ${.CURDIR}/rsa_openssh.prv

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

.for t in ${LTESTS}
REGRESS_TARGETS+=t-${t}
t-${t}:
	sh ${.CURDIR}/test-exec.sh ${.OBJDIR} ${.CURDIR}/${t}.sh
.endfor

.include <bsd.regress.mk>
