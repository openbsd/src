#	$OpenBSD: Makefile,v 1.2 1996/06/26 05:41:52 deraadt Exp $

MAN=	true.1

beforeinstall:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/true.sh ${DESTDIR}${BINDIR}/true

.include <bsd.prog.mk>
