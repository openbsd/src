#	$OpenBSD: Makefile,v 1.2 1997/08/01 12:32:10 pefo Exp $

CLEANFILES= mbr msdos5mb.gz

all: mbr msdos5mb.gz

mbr: mbr.uu
	uudecode ${.CURDIR}/mbr.uu

msdos5mb.gz: msdos5mb.gz.uu
	uudecode ${.CURDIR}/msdos5mb.gz.uu

realinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 mbr msdos5mb.gz \
		${DESTDIR}/usr/mdec


.include <bsd.prog.mk>
