# $OpenBSD: Makefile,v 1.2 2004/11/16 23:58:18 deraadt Exp $

# USB capable systems only
.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "alpha") || (${MACHINE} == "sparc") || \
    (${MACHINE} == "sparc64") || (${MACHINE_ARCH} == "powerpc") || \
    (${MACHINE} == "cats") || (${MACHINE} == "hppa")

FIRM= 	atu-intersil-int atu-rfmd-int atu-rfmd2958-int \
	atu-intersil-ext atu-rfmd-ext atu-rfmd2958-ext
CLEANFILES+= ${FIRM} build

all:	build
	${.OBJDIR}/build

NOPROG=
NOMAN=

.include <bsd.prog.mk>

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${FIRM} ${DESTDIR}/etc/firmware
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/atu-license ${DESTDIR}/etc/firmware


.endif
