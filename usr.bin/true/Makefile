#	$OpenBSD: Makefile,v 1.4 1997/04/27 20:56:55 millert Exp $

MAN=	true.1

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/true.sh ${DESTDIR}${BINDIR}/true

.include <bsd.prog.mk>
