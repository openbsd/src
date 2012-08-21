#	$OpenBSD: Makefile,v 1.2 2012/08/21 14:46:18 pascal Exp $
#	$NetBSD: Makefile,v 1.1 1997/02/04 03:52:59 thorpej Exp $

NOMAN=
NOPIE=

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/installboot.sh ${DESTDIR}${BINDIR}/installboot

.include <bsd.prog.mk>
