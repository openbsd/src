#	$OpenBSD: Makefile,v 1.17 2019/10/31 21:22:01 djm Exp $

.include <bsd.own.mk>

SUBDIR=	ssh sshd ssh-add ssh-keygen ssh-agent scp sftp-server \
	ssh-keysign ssh-keyscan sftp ssh-pkcs11-helper ssh-sk-helper

distribution:
	${INSTALL} -C -o root -g wheel -m 0644 ${.CURDIR}/ssh_config \
	    ${DESTDIR}/etc/ssh/ssh_config
	${INSTALL} -C -o root -g wheel -m 0644 ${.CURDIR}/sshd_config \
	    ${DESTDIR}/etc/ssh/sshd_config

.include <bsd.subdir.mk>
