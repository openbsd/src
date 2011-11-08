#	$OpenBSD: Makefile,v 1.7 2011/11/08 10:37:10 guenther Exp $

PROG=	xinstall
SRCS=	xinstall.c
MAN=	install.1

realinstall:
	${INSTALL} ${INSTALL_COPY} -S ${INSTALL_STRIP} \
	    -o ${BINOWN} -g ${BINGRP} \
	    -m ${BINMODE} ${PROG} ${DESTDIR}${BINDIR}/install

.include <bsd.prog.mk>
