# $OpenBSD: Makefile,v 1.5 2004/12/31 00:28:31 deraadt Exp $

NOPROG=
NOMAN=

# pcmcia capable systems only
.if (${MACHINE} == "i386") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "sparc") || (${MACHINE} == "sparc64") || \
    (${MACHINE} == "hppa") || (${MACHINE} == "hppa64") || \
    (${MACHINE} == "sgi") || (${MACHINE} == "zaurus")

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/eprimsym ${DESTDIR}/etc/firmware/symbol-eprim
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/esecsym ${DESTDIR}/etc/firmware/symbol-esec
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/symbol-license ${DESTDIR}/etc/firmware
.endif

.include <bsd.prog.mk>

