# $OpenBSD: Makefile,v 1.1 2004/11/16 23:26:43 deraadt Exp $

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


