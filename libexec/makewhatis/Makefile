#	$OpenBSD: Makefile,v 1.6 2000/02/03 18:10:48 espie Exp $

MAN=makewhatis.8
NOPROG=

afterinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/makewhatis.pl ${DESTDIR}${BINDIR}/makewhatis

.include <bsd.prog.mk>
