#	$OpenBSD: Makefile,v 1.1 2003/07/10 16:06:07 millert Exp $

PROG=	diff3prog
MAN=	diff3.1
BINDIR=	/usr/libexec

beforeinstall:
	install ${INSTALL_COPY}c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/diff3.ksh ${DESTDIR}/usr/bin/${PROG}

.include <bsd.prog.mk>
