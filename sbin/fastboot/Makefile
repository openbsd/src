#	$OpenBSD: Makefile,v 1.5 1997/04/27 20:56:21 millert Exp $
#	$NetBSD: Makefile,v 1.7 1995/03/18 14:55:27 cgd Exp $

MAN=	fastboot.8
MLINKS=	fastboot.8 fasthalt.8

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/fastboot.sh ${DESTDIR}${BINDIR}/fastboot
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/fasthalt.sh ${DESTDIR}${BINDIR}/fasthalt

.include <bsd.prog.mk>
