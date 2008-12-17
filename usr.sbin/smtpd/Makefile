#	$OpenBSD: Makefile,v 1.8 2008/12/17 22:59:36 jacekm Exp $

.include <bsd.own.mk>

SUBDIR = makemap smtpd smtpctl

distribution:
	${INSTALL} -C -o root -g wheel -m 0644 ${.CURDIR}/smtpd.conf \
		${DESTDIR}/etc/mail/smtpd.conf

.include <bsd.subdir.mk>
