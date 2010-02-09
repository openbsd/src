#	$OpenBSD: Makefile,v 1.15 2010/02/09 08:55:31 markus Exp $

.include <bsd.own.mk>

SUBDIR=	lib ssh sshd ssh-add ssh-keygen ssh-agent scp sftp-server \
	ssh-keysign ssh-keyscan sftp ssh-pkcs11-helper

distribution:
	${INSTALL} -C -o root -g wheel -m 0644 ${.CURDIR}/ssh_config \
	    ${DESTDIR}/etc/ssh/ssh_config
	${INSTALL} -C -o root -g wheel -m 0644 ${.CURDIR}/sshd_config \
	    ${DESTDIR}/etc/ssh/sshd_config

.include <bsd.subdir.mk>
