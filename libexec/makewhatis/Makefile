#	$OpenBSD: Makefile,v 1.2 1996/12/08 15:17:19 downsj Exp $

NOMAN=
NOPROG=
NOOBJ=

afterinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/makewhatis.sh ${DESTDIR}${BINDIR}/makewhatis

.include <bsd.prog.mk>
