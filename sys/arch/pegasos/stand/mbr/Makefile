#	$OpenBSD: Makefile,v 1.1 2003/10/31 03:54:34 drahn Exp $

CLEANFILES= mbr

all: mbr

mbr:
	dd if=/dev/zero of=mbr bs=512 count=1

realinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 mbr ${DESTDIR}/usr/mdec


.include <bsd.prog.mk>
