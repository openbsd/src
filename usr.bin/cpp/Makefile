#	$OpenBSD: Makefile,v 1.4 1996/08/26 21:25:22 niklas Exp $

NOMAN=

beforeinstall:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/cpp.sh ${DESTDIR}${BINDIR}/cpp

.include <bsd.prog.mk>
