#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:50:26 deraadt Exp $

MAN=	pagesize.1

SCRIPT=	pagesize.sh

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${SCRIPT} ${DESTDIR}${BINDIR}/pagesize

.include <bsd.prog.mk>
