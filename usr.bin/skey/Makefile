# $OpenBSD: Makefile,v 1.10 1997/07/17 05:48:39 millert Exp $

PROG=	skey
MAN=	skey.1 skeyinfo.1 skeyaudit.1 skeyprune.8
LINKS=	${BINDIR}/skey ${BINDIR}/otp-md4 \
	${BINDIR}/skey ${BINDIR}/otp-md5 \
	${BINDIR}/skey ${BINDIR}/otp-sha1 \
	${BINDIR}/skey ${BINDIR}/otp-rmd160
MLINKS=	skey.1 otp-md4.1 \
	skey.1 otp-md5.1 \
	skey.1 otp-sha1.1 \
	skey.1 otp-rmd160.1
DPADD=	${LIBSKEY}
LDADD=	-lskey

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -m 755 ${.CURDIR}/skeyaudit.sh \
		${DESTDIR}${BINDIR}/skeyaudit
	${INSTALL} ${INSTALL_COPY} -m 755 ${.CURDIR}/skeyinfo.sh \
		${DESTDIR}${BINDIR}/skeyinfo
	${INSTALL} ${INSTALL_COPY} -m 755 ${.CURDIR}/skeyprune.pl \
		${DESTDIR}${BINDIR}/skeyprune

.include <bsd.prog.mk>
