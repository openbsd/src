#	$OpenBSD: Makefile,v 1.3 1996/12/08 14:32:40 downsj Exp $
#	$NetBSD: Makefile,v 1.7 1994/12/22 12:31:39 cgd Exp $

PROG=	xinstall
SRCS=	stat_flags.c xinstall.c
MAN=	install.1
.PATH:	${.CURDIR}/../../bin/ls

realinstall:
	${INSTALL} ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${PROG} ${DESTDIR}${BINDIR}/install

.include <bsd.prog.mk>
