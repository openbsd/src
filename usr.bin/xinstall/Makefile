#	$OpenBSD: Makefile,v 1.4 1997/04/27 20:56:59 millert Exp $
#	$NetBSD: Makefile,v 1.7 1994/12/22 12:31:39 cgd Exp $

PROG=	xinstall
SRCS=	stat_flags.c xinstall.c
MAN=	install.1
.PATH:	${.CURDIR}/../../bin/ls

realinstall:
	${INSTALL} ${INSTALL_COPY} ${INSTALL_STRIP} -o ${BINOWN} -g ${BINGRP} \
	    -m ${BINMODE} ${PROG} ${DESTDIR}${BINDIR}/install

.include <bsd.prog.mk>
