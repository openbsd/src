#	$OpenBSD: Makefile,v 1.4 1997/11/05 19:20:46 deraadt Exp $

MAN=makewhatis.8
NOPROG=
NOOBJ=

afterinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/makewhatis.sh ${DESTDIR}${BINDIR}/makewhatis

.include <bsd.prog.mk>
