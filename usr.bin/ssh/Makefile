#	$OpenBSD: Makefile,v 1.12 2003/12/01 15:47:20 grange Exp $

.include <bsd.own.mk>

SUBDIR=	lib ssh sshd ssh-add ssh-keygen ssh-agent scp sftp-server \
	ssh-keysign ssh-keyscan sftp scard

distribution:
	${INSTALL} -C -o root -g wheel -m 0644 ${.CURDIR}/ssh_config \
	    ${DESTDIR}/etc/ssh/ssh_config
	${INSTALL} -C -o root -g wheel -m 0644 ${.CURDIR}/sshd_config \
	    ${DESTDIR}/etc/ssh/sshd_config

.include <bsd.subdir.mk>
