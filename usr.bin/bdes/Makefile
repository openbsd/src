#	$NetBSD: Makefile,v 1.1 1995/07/24 04:30:47 cgd Exp $

MAN=	bdes.1

SCRIPT=	bdes.sh

realinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${SCRIPT} ${DESTDIR}${BINDIR}/bdes

.include <bsd.prog.mk>
