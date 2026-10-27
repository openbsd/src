# $OpenBSD: Makefile,v 1.1 2026/10/27 01:38:20 kevlo Exp $

NOPROG=
NOMAN=
CC=${HOSTCC}

FIRM=	rge-rtl8125 rge-rtl8125b rge-rtl8125d-1 rge-rtl8125d-2 rge-rtl8126-1 \
	rge-rtl8126-2 rge-rtl8127

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
	    ${.CURDIR}/rge-license ${DESTDIR}/etc/firmware

.include <bsd.prog.mk>
