#	$OpenBSD: Makefile,v 1.5 1997/11/08 07:09:30 todd Exp $

MAN=makewhatis.8
NOPROG=

afterinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/makewhatis.sh ${DESTDIR}${BINDIR}/makewhatis

.include <bsd.prog.mk>
