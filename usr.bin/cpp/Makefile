#	$OpenBSD: Makefile,v 1.7 1999/12/23 07:14:12 mickey Exp $

NOMAN=
NOOBJ=

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/cpp.sh ${DESTDIR}${BINDIR}/cpp

.include <bsd.prog.mk>
