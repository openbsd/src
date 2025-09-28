# $OpenBSD: Makefile,v 1.14 2025/09/28 12:30:49 sthen Exp $

all clean:

distribution:
	${INSTALL} -c -o root -g wheel -m 644 aliases \
	    ${DESTDIR}/etc/mail/aliases
	${INSTALL} -c -o root -g wheel -m 644 smtpd.conf \
	    ${DESTDIR}/etc/mail/smtpd.conf

.PHONY: distribution
.include <bsd.own.mk>
.include <bsd.sys.mk>
