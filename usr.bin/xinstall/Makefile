#	$OpenBSD: Makefile,v 1.6 2000/07/19 19:29:16 mickey Exp $

PROG=	xinstall
SRCS=	xinstall.c
MAN=	install.1

realinstall:
	${INSTALL} ${INSTALL_COPY} ${INSTALL_STRIP} -o ${BINOWN} -g ${BINGRP} \
	    -m ${BINMODE} ${PROG} ${DESTDIR}${BINDIR}/install

.include <bsd.prog.mk>
