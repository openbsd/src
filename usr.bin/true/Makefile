#	$OpenBSD: Makefile,v 1.3 1996/12/08 14:32:36 downsj Exp $

MAN=	true.1

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/true.sh ${DESTDIR}${BINDIR}/true

.include <bsd.prog.mk>
