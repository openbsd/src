# $OpenBSD: Makefile,v 1.1 2004/11/22 09:10:11 deraadt Exp $

NOPROG=
NOMAN=

# pcmcia capable systems only
.if (${MACHINE} == "i386") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "sparc")

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/eprimsym ${DESTDIR}/etc/firmware
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/esecsym ${DESTDIR}/etc/firmware
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/symbol-license ${DESTDIR}/etc/firmware
.endif

.include <bsd.prog.mk>

