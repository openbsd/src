#	$OpenBSD: Makefile,v 1.19 2024/10/14 01:57:50 djm Exp $

.include <bsd.own.mk>

SUBDIR=	ssh sshd sshd-session sshd-auth \
	ssh-add ssh-keygen ssh-agent scp sftp-server \
	ssh-keysign ssh-keyscan sftp ssh-pkcs11-helper ssh-sk-helper

distribution:
	${INSTALL} -C -o root -g wheel -m 0644 ${.CURDIR}/ssh_config \
	    ${DESTDIR}/etc/ssh/ssh_config
	${INSTALL} -C -o root -g wheel -m 0644 ${.CURDIR}/sshd_config \
	    ${DESTDIR}/etc/ssh/sshd_config

.include <bsd.subdir.mk>
