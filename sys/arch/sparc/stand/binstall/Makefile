#	$OpenBSD: Makefile,v 1.1 1997/09/17 10:46:13 downsj Exp $

NOMAN=	noman

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/binstall.sh ${DESTDIR}${BINDIR}/binstall

.include <bsd.prog.mk>
