#	$OpenBSD: Makefile,v 1.3 2012/09/02 23:23:51 pascal Exp $

NOMAN=	noman

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/binstall.sh ${DESTDIR}${BINDIR}/binstall

.include <bsd.prog.mk>
