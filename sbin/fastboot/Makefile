#	$OpenBSD: Makefile,v 1.3 1996/12/08 14:01:44 downsj Exp $
#	$NetBSD: Makefile,v 1.7 1995/03/18 14:55:27 cgd Exp $

MAN=	fastboot.8
MLINKS=	fastboot.8 fasthalt.8

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/fastboot.sh ${DESTDIR}${BINDIR}/fastboot
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/fasthalt.sh ${DESTDIR}${BINDIR}/fasthalt

.include <bsd.prog.mk>
