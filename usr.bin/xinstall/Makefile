#	$OpenBSD: Makefile,v 1.2 1996/06/26 05:44:04 deraadt Exp $
#	$NetBSD: Makefile,v 1.7 1994/12/22 12:31:39 cgd Exp $

PROG=	xinstall
SRCS=	stat_flags.c xinstall.c
MAN=	install.1
.PATH:	${.CURDIR}/../../bin/ls

realinstall:
	install ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${PROG} ${DESTDIR}${BINDIR}/install

.include <bsd.prog.mk>
