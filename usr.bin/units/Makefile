#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:46:30 deraadt Exp $

PROG=	units

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/units.lib ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
