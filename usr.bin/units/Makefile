#	$OpenBSD: Makefile,v 1.4 1997/04/27 20:56:56 millert Exp $

PROG=	units

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/units.lib ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
