# $OpenBSD: Makefile,v 1.1 2006/06/26 14:24:26 deraadt Exp $

NOMAN=

FIRM=	zd1211

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
	    ${.CURDIR}/zd1211-licence ${DESTDIR}/etc/firmware


.include <bsd.prog.mk>
