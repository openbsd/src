# $OpenBSD: Makefile,v 1.6 1996/09/29 23:04:58 millert Exp $

PROG=	skey
MAN=	skey.1 skeyinfo.1 skeyaudit.1 skeyprune.8
LINKS=	/usr/bin/skey /usr/bin/otp-md4 \
	/usr/bin/skey /usr/bin/otp-md5 \
	/usr/bin/skey /usr/bin/otp-sha1
MLINKS=	skey.1 otp-md4.1 \
	skey.1 otp-md5.1 \
	skey.1 otp-sha1.1
DPADD=	${LIBSKEY}
LDADD=	-lskey

beforeinstall:
	install -c -m 755 ${.CURDIR}/skeyaudit.sh ${DESTDIR}${BINDIR}/skeyaudit
	install -c -m 755 ${.CURDIR}/skeyinfo.sh ${DESTDIR}${BINDIR}/skeyinfo
	install -c -m 755 ${.CURDIR}/skeyprune.pl ${DESTDIR}${BINDIR}/skeyprune

.include <bsd.prog.mk>
