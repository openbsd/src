#	$OpenBSD: Makefile,v 1.6 2008/12/04 15:16:08 gilles Exp $

.include <bsd.own.mk>

SUBDIR = makemap newaliases smtpd

distribution:
	${INSTALL} -C -o root -g wheel -m 0644 ${.CURDIR}/smtpd.conf \
		${DESTDIR}/etc/mail/smtpd.conf

.include <bsd.subdir.mk>
