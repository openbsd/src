#	$OpenBSD: Makefile,v 1.3 1996/12/08 14:32:37 downsj Exp $

PROG=	units

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/units.lib ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
