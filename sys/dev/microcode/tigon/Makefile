# $OpenBSD: Makefile,v 1.7 2009/08/29 21:37:14 kettenis Exp $

NOPROG=
NOMAN=
CC=${HOSTCC}

# PCI & SBus capable systems only
.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "alpha") || (${MACHINE} == "sparc64") || \
    (${MACHINE} == "sparc") || (${MACHINE_ARCH} == "powerpc") || \
    (${MACHINE} == "hppa") ||  (${MACHINE} == "hppa64") || \
    (${MACHINE} == "sgi")

FIRM= 	tigon1 tigon2

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
	    ${.CURDIR}/tigon-license ${DESTDIR}/etc/firmware
.endif

.include <bsd.prog.mk>
