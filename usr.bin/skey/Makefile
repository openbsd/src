#	$OpenBSD: Makefile,v 1.13 1997/09/21 11:50:50 deraadt Exp $

PROG=	skey
MAN=	skey.1 skeyprune.8
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
	${INSTALL} ${INSTALL_COPY} -m 755 ${.CURDIR}/skeyprune.pl \
		${DESTDIR}${BINDIR}/skeyprune

.include <bsd.prog.mk>
