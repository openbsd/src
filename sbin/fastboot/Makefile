#	$NetBSD: Makefile,v 1.7 1995/03/18 14:55:27 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93

MAN=	fastboot.8
MLINKS=	fastboot.8 fasthalt.8

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/fastboot.sh ${DESTDIR}${BINDIR}/fastboot
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/fasthalt.sh ${DESTDIR}${BINDIR}/fasthalt

.include <bsd.prog.mk>
