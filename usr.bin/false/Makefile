#	$OpenBSD: Makefile,v 1.2 1996/06/26 05:32:49 deraadt Exp $

MAN=	false.1

beforeinstall:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/false.sh ${DESTDIR}${BINDIR}/false

.include <bsd.prog.mk>
