#	$OpenBSD: Makefile,v 1.7 2000/12/04 19:24:01 markus Exp $

.include <bsd.own.mk>

SUBDIR=	lib ssh sshd ssh-add ssh-keygen ssh-agent scp sftp-server \
	ssh-keyscan

distribution:
	install -C -o root -g wheel -m 0644 ${.CURDIR}/ssh_config \
	    ${DESTDIR}/etc/ssh_config
	install -C -o root -g wheel -m 0644 ${.CURDIR}/sshd_config \
	    ${DESTDIR}/etc/sshd_config

.include <bsd.subdir.mk>
