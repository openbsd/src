#	$OpenBSD: Makefile,v 1.2 1996/06/26 05:37:42 deraadt Exp $
#	$NetBSD: Makefile,v 1.6 1995/09/05 01:11:10 cgd Exp $

MAN=	pagesize.1

SCRIPT=	pagesize.sh

realinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${SCRIPT} ${DESTDIR}${BINDIR}/pagesize

.include <bsd.prog.mk>
