#	$OpenBSD: Makefile,v 1.2 2003/07/22 00:11:30 millert Exp $

PROG=	diff3prog
MAN=	diff3.1
BINDIR=	/usr/libexec

beforeinstall:
	install ${INSTALL_COPY}c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/diff3.ksh ${DESTDIR}/usr/bin/diff3

.include <bsd.prog.mk>
