#	$OpenBSD: Makefile,v 1.2 2012/08/21 14:46:19 pascal Exp $

CLEANFILES= mbr msdos1mb.gz
NOPIE=

all: mbr msdos1mb.gz

mbr: mbr.uu
	uudecode ${.CURDIR}/mbr.uu

msdos1mb.gz: msdos1mb.gz.uu
	uudecode ${.CURDIR}/msdos1mb.gz.uu

realinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 mbr msdos1mb.gz \
		${DESTDIR}/usr/mdec


.include <bsd.prog.mk>
