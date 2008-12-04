#	$OpenBSD: Makefile,v 1.5 2008/12/04 13:36:58 todd Exp $

.include <bsd.own.mk>

SUBDIRS = makemap newaliases smtpd

distribution:
	${INSTALL} -C -o root -g wheel -m 0644 ${.CURDIR}/smtpd.conf \
		${DESTDIR}/etc/mail/smtpd.conf

.include <bsd.subdir.mk>
