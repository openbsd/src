# $OpenBSD: Makefile,v 1.13 2016/09/03 13:37:45 guenther Exp $

NOPROG=
NOMAN=
CC=${HOSTCC}

# pcmcia capable systems only
.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "macppc") || \
    (${MACHINE} == "sparc64") || (${MACHINE} == "hppa") || \
    (${MACHINE} == "sgi")

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/eprimsym ${DESTDIR}/etc/firmware/symbol-eprim
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/esecsym ${DESTDIR}/etc/firmware/symbol-esec
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/symbol-license ${DESTDIR}/etc/firmware
.endif

.include <bsd.prog.mk>
