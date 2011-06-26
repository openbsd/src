# $OpenBSD: Makefile,v 1.2 2011/06/26 20:50:35 deraadt Exp $

NOPROG=
NOMAN=
CC=${HOSTCC}

# full-size PCI capable systems only
.if (${MACHINE} == "macppc") || (${MACHINE} == "alpha") || \
    (${MACHINE} == "amd64") || (${MACHINE} == "hppa") || \
    (${MACHINE} == "hppa64") || (${MACHINE} == "i386") || \
    (${MACHINE} == "sgi") || (${MACHINE} == "sparc64")

FIRM=	myx-ethp_z8e myx-eth_z8e

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
	    ${.CURDIR}/myx-license ${DESTDIR}/etc/firmware

.endif

.include <bsd.prog.mk>
