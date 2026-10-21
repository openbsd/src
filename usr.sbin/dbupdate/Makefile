#	$OpenBSD: Makefile,v 1.1 2026/10/21 01:08:58 deraadt Exp $

SCRIPT=	dbupdate.sh
MAN=	dbupdate.8

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${SCRIPT} ${DESTDIR}${BINDIR}/dbupdate

.include <bsd.prog.mk>
