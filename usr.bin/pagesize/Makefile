#	@(#)Makefile	8.2 (Berkeley) 4/3/94
#	$NetBSD: Makefile,v 1.6 1995/09/05 01:11:10 cgd Exp $

MAN=	pagesize.1

SCRIPT=	pagesize.sh

realinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${SCRIPT} ${DESTDIR}${BINDIR}/pagesize

.include <bsd.prog.mk>
