#	$OpenBSD: Makefile,v 1.4 1997/04/27 20:56:42 millert Exp $

MAN=	false.1

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/false.sh ${DESTDIR}${BINDIR}/false

.include <bsd.prog.mk>
