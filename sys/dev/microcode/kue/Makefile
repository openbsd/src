# $OpenBSD: Makefile,v 1.9 2009/09/09 12:10:35 marex Exp $

NOPROG=
NOMAN=
CC=${HOSTCC}

# USB capable systems only
.if (${MACHINE_ARCH} == "powerpc") || (${MACHINE} == "alpha") || \
    (${MACHINE} == "amd64") || (${MACHINE} == "armish") || \
    (${MACHINE} == "hppa") || (${MACHINE} == "hppa64") || \
    (${MACHINE} == "i386") || (${MACHINE} == "sgi") || \
    (${MACHINE} == "sparc64") || (${MACHINE} == "zaurus") || \
    (${MACHINE} == "landisk") || (${MACHINE} == "palm")

FIRM= 	kue

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
	    ${.CURDIR}/kue-license ${DESTDIR}/etc/firmware
.endif

.include <bsd.prog.mk>
