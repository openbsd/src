#	$OpenBSD: Makefile,v 1.3 1996/12/08 14:32:32 downsj Exp $
#	$NetBSD: Makefile,v 1.6 1995/09/05 01:11:10 cgd Exp $

MAN=	pagesize.1

SCRIPT=	pagesize.sh

realinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${SCRIPT} ${DESTDIR}${BINDIR}/pagesize

.include <bsd.prog.mk>
