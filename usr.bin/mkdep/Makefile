#	$NetBSD: Makefile,v 1.6 1994/12/23 07:34:55 jtc Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

MAN=	mkdep.1

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/mkdep.gcc.sh ${DESTDIR}/usr/bin/mkdep

.include <bsd.prog.mk>
