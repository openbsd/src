#	$OpenBSD: Makefile,v 1.3 1996/12/08 14:32:39 downsj Exp $

MAN=	which.1

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/which.csh ${DESTDIR}${BINDIR}/which

.include <bsd.prog.mk>
