#	$OpenBSD: Makefile,v 1.5 1996/12/08 14:32:21 downsj Exp $

NOMAN=

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/cpp.sh ${DESTDIR}${BINDIR}/cpp

.include <bsd.prog.mk>
