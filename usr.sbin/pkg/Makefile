#	$OpenBSD: Makefile,v 1.2 2001/04/02 17:25:53 espie Exp $

NOPROG=
MAN=	pkg.1

afterinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/pkg.pl ${DESTDIR}${BINDIR}/pkg

.include <bsd.prog.mk>
