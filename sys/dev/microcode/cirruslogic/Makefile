# $OpenBSD: Makefile,v 1.1 2004/12/19 16:06:23 deraadt Exp $

NOPROG=
NOMAN=

# PCI capable systems only
.if (${MACHINE} == "i386")

FIRM=	cs4280

CLEANFILES+= build ${FIRM}

all:	build
	${.OBJDIR}/build

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${FIRM} ${DESTDIR}/etc/firmware
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/cs4280-license ${DESTDIR}/etc/firmware
.endif

.include <bsd.prog.mk>

