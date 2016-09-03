# $OpenBSD: Makefile,v 1.10 2016/09/03 13:37:45 guenther Exp $

NOPROG=
NOMAN=
CC=${HOSTCC}

# USB capable systems only
.if (${MACHINE_ARCH} == "powerpc" || \
    ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "armv7" || \
    ${MACHINE} == "hppa" || ${MACHINE} == "i386" || \
    ${MACHINE} == "landisk" || ${MACHINE} == "loongson" || \
    ${MACHINE} == "octeon" || ${MACHINE} == "sgi" || \
    ${MACHINE} == "sparc64")

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
