#	$OpenBSD: Makefile,v 1.15 2016/03/30 06:38:46 jmc Exp $

PROG=	skey
MAN=	skey.1 skeyprune.8
LINKS=	${BINDIR}/skey ${BINDIR}/otp-md5 \
	${BINDIR}/skey ${BINDIR}/otp-sha1 \
	${BINDIR}/skey ${BINDIR}/otp-rmd160
DPADD=	${LIBSKEY}
LDADD=	-lskey

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -m 755 ${.CURDIR}/skeyprune.pl \
		${DESTDIR}${BINDIR}/skeyprune

.include <bsd.prog.mk>
