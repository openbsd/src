# $OpenBSD: Makefile,v 1.2 2005/06/16 08:23:49 markus Exp $

NOPROG=
NOMAN=

.if (${MACHINE} == "i386")

FIRM= 	neo-coefficients

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
	    ${.CURDIR}/neo-license ${DESTDIR}/etc/firmware
.endif

.include <bsd.prog.mk>
