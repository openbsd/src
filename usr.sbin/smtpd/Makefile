#	$OpenBSD: Makefile,v 1.7 2008/12/05 03:28:37 gilles Exp $

.include <bsd.own.mk>

SUBDIR = makemap newaliases smtpd smtpctl

distribution:
	${INSTALL} -C -o root -g wheel -m 0644 ${.CURDIR}/smtpd.conf \
		${DESTDIR}/etc/mail/smtpd.conf

.include <bsd.subdir.mk>
