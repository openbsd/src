# $OpenBSD: Makefile,v 1.1 2004/11/22 04:14:18 deraadt Exp $

NOPROG=
NOMAN=

# PCI capable systems only
.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "alpha") || (${MACHINE} == "sparc64") || \
    (${MACHINE_ARCH} == "powerpc") || (${MACHINE} == "cats") || \
    (${MACHINE} == "hppa") || (${MACHINE} == "sgi")

FIRM= 	tigon1 tigon2

CLEANFILES+= ${FIRM} build

all:	build
	${.OBJDIR}/build

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${FIRM} ${DESTDIR}/etc/firmware
.endif

.include <bsd.prog.mk>

