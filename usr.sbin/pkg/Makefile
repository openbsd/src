#	$OpenBSD: Makefile,v 1.1 2001/04/02 10:11:16 espie Exp $

NOMAN=
NOPROG=

afterinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/pkg.pl ${DESTDIR}${BINDIR}/pkg

.include <bsd.prog.mk>
