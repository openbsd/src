#	$OpenBSD: Makefile,v 1.2 2012/08/21 14:46:19 pascal Exp $

NOMAN=	noman
NOPIE=

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/binstall.sh ${DESTDIR}${BINDIR}/binstall

.include <bsd.prog.mk>
