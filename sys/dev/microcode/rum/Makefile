# $OpenBSD: Makefile,v 1.5 2008/06/29 23:57:58 jsg Exp $

NOPROG=
NOMAN=
CC=${HOSTCC}

# USB capable systems only
.if (${MACHINE_ARCH} == "powerpc") || (${MACHINE} == "alpha") || \
    (${MACHINE} == "amd64") || (${MACHINE} == "armish") || \
    (${MACHINE} == "hppa") || (${MACHINE} == "hppa64") || \
    (${MACHINE} == "i386") || (${MACHINE} == "sgi") || \
    (${MACHINE} == "sparc64") || (${MACHINE} == "zaurus") || \
    (${MACHINE} == "landisk")

FIRM=	rum-rt2573 run-rt2870

PROG=	build

CLEANFILES+= ${FIRM} ${PROG}

all: ${FIRM}

${FIRM}: build
	${.OBJDIR}/build

realinstall:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${FIRM} ${DESTDIR}/etc/firmware
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/rum-license ${DESTDIR}/etc/firmware

.endif

.include <bsd.prog.mk>
