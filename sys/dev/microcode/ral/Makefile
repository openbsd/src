# $OpenBSD: Makefile,v 1.10 2016/07/10 15:16:15 visa Exp $

NOPROG=
NOMAN=
CC=${HOSTCC}

# PCI capable systems only
.if (${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
     ${MACHINE} == "armish" || ${MACHINE} == "hppa" || \
     ${MACHINE} == "i386" || \
     ${MACHINE} == "loongson" || ${MACHINE} == "octeon" || \
     ${MACHINE_ARCH} == "powerpc" || \
     ${MACHINE} == "sgi" || ${MACHINE} == "sparc64")

FIRM=	ral-rt2561 ral-rt2561s ral-rt2661 ral-rt2860

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
	    ${.CURDIR}/ral-license ${DESTDIR}/etc/firmware

.endif

.include <bsd.prog.mk>
