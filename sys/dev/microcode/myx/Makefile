# $OpenBSD: Makefile,v 1.3 2011/07/08 03:58:27 dlg Exp $

NOPROG=
NOMAN=
CC=${HOSTCC}
LDADD=-lz
DPADD=${LIBZ}

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
