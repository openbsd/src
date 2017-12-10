#	$OpenBSD: Makefile,v 1.16 2017/12/10 19:37:57 deraadt Exp $

.include <bsd.own.mk>

SUBDIR=	ssh sshd ssh-add ssh-keygen ssh-agent scp sftp-server \
	ssh-keysign ssh-keyscan sftp ssh-pkcs11-helper

distribution:
	${INSTALL} -C -o root -g wheel -m 0644 ${.CURDIR}/ssh_config \
	    ${DESTDIR}/etc/ssh/ssh_config
	${INSTALL} -C -o root -g wheel -m 0644 ${.CURDIR}/sshd_config \
	    ${DESTDIR}/etc/ssh/sshd_config

.include <bsd.subdir.mk>
