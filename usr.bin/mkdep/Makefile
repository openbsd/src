#	$OpenBSD: Makefile,v 1.2 1996/06/26 05:37:06 deraadt Exp $
#	$NetBSD: Makefile,v 1.6 1994/12/23 07:34:55 jtc Exp $

MAN=	mkdep.1

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/mkdep.gcc.sh ${DESTDIR}/usr/bin/mkdep

.include <bsd.prog.mk>
