#	$OpenBSD: Makefile,v 1.3 1996/12/08 14:32:23 downsj Exp $

MAN=	false.1

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/false.sh ${DESTDIR}${BINDIR}/false

.include <bsd.prog.mk>
