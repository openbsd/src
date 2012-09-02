#	$OpenBSD: Makefile,v 1.3 2012/09/02 23:07:26 deraadt Exp $

CLEANFILES= mbr msdos1mb.gz

all: mbr msdos1mb.gz

mbr: mbr.uu
	uudecode ${.CURDIR}/mbr.uu

msdos1mb.gz: msdos1mb.gz.uu
	uudecode ${.CURDIR}/msdos1mb.gz.uu

realinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 mbr msdos1mb.gz \
		${DESTDIR}/usr/mdec


.include <bsd.prog.mk>
