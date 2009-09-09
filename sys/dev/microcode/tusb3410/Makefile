# $OpenBSD: Makefile,v 1.3 2009/09/09 12:10:35 marex Exp $

NOPROG=
NOMAN=
CC=${HOSTCC}

# USB capable systems only
.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "alpha") || (${MACHINE} == "sparc64") || \
    (${MACHINE_ARCH} == "powerpc") || (${MACHINE} == "hppa") || \
    (${MACHINE} == "hppa64") || (${MACHINE} == "sgi") || \
    (${MACHINE} == "landisk") || (${MACHINE} == "armish") || \
    (${MACHINE} == "zaurus") || (${MACHINE} == "palm")

FIRM= 	tusb3410

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
	    ${.CURDIR}/tusb3410-license ${DESTDIR}/etc/firmware
.endif

.include <bsd.prog.mk>
