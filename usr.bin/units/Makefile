#	$OpenBSD: Makefile,v 1.2 1996/04/21 23:44:23 deraadt Exp $

PROG=	units

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/units.lib ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
