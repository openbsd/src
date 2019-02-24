#	$OpenBSD: Makefile,v 1.10 2019/02/24 12:57:14 kn Exp $

MAN=	lorder.1

SCRIPT=	lorder.sh

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${SCRIPT} ${DESTDIR}${BINDIR}/lorder

.include <bsd.prog.mk>
