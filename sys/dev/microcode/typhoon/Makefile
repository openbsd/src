# $OpenBSD: Makefile,v 1.3 2004/12/19 16:19:43 deraadt Exp $

NOPROG=
NOMAN=

# PCI capable systems only
.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "alpha") || (${MACHINE} == "sparc64") || \
    (${MACHINE_ARCH} == "powerpc") || (${MACHINE} == "cats") || \
    (${MACHINE} == "hppa") || (${MACHINE} == "hppa64") || \
    (${MACHINE} == "sgi")

FIRM=	3c990

CLEANFILES+= build ${FIRM}

all:	build
	${.OBJDIR}/build

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${FIRM} ${DESTDIR}/etc/firmware
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/3c990-license ${DESTDIR}/etc/firmware
.endif

.include <bsd.prog.mk>

