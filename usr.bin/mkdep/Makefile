#	$OpenBSD: Makefile,v 1.4 1997/04/27 20:56:50 millert Exp $
#	$NetBSD: Makefile,v 1.6 1994/12/23 07:34:55 jtc Exp $

MAN=	mkdep.1

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/mkdep.gcc.sh ${DESTDIR}/usr/bin/mkdep

.include <bsd.prog.mk>
