#	$OpenBSD: Makefile,v 1.16 2016/09/11 13:51:30 natano Exp $

PROG=	skey
MAN=	skey.1 skeyprune.8
LINKS=	${BINDIR}/skey ${BINDIR}/otp-md5 \
	${BINDIR}/skey ${BINDIR}/otp-sha1 \
	${BINDIR}/skey ${BINDIR}/otp-rmd160
DPADD=	${LIBSKEY}
LDADD=	-lskey

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 755 \
	    ${.CURDIR}/skeyprune.pl ${DESTDIR}${BINDIR}/skeyprune

.include <bsd.prog.mk>
