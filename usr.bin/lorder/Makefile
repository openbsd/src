#	$OpenBSD: Makefile,v 1.8 1999/05/21 01:24:04 espie Exp $

MAN=	lorder.1

SCRIPT=	lorder.sh

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${SCRIPT} ${DESTDIR}${BINDIR}/lorder

.include <bsd.prog.mk>
