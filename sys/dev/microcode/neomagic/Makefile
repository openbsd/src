# $OpenBSD: Makefile,v 1.1 2004/11/22 04:29:06 deraadt Exp $

NOPROG=
NOMAN=

.if (${MACHINE} == "i386")

FIRM= 	neo-coefficients

CLEANFILES+= ${FIRM} build

all:	build
	${.OBJDIR}/build

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${FIRM} ${DESTDIR}/etc/firmware
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/neo-license ${DESTDIR}/etc/firmware
.endif

.include <bsd.prog.mk>

