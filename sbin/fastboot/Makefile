#	$OpenBSD: Makefile,v 1.2 1996/06/23 14:30:22 deraadt Exp $
#	$NetBSD: Makefile,v 1.7 1995/03/18 14:55:27 cgd Exp $

MAN=	fastboot.8
MLINKS=	fastboot.8 fasthalt.8

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/fastboot.sh ${DESTDIR}${BINDIR}/fastboot
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/fasthalt.sh ${DESTDIR}${BINDIR}/fasthalt

.include <bsd.prog.mk>
