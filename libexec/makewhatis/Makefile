#	$OpenBSD: Makefile,v 1.3 1997/04/27 20:56:20 millert Exp $

NOMAN=
NOPROG=
NOOBJ=

afterinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/makewhatis.sh ${DESTDIR}${BINDIR}/makewhatis

.include <bsd.prog.mk>
