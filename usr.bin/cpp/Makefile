#	$OpenBSD: Makefile,v 1.6 1997/04/27 20:56:42 millert Exp $

NOMAN=

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/cpp.sh ${DESTDIR}${BINDIR}/cpp

.include <bsd.prog.mk>
