#	$OpenBSD: Makefile,v 1.3 1996/12/08 14:32:30 downsj Exp $
#	$NetBSD: Makefile,v 1.6 1994/12/23 07:34:55 jtc Exp $

MAN=	mkdep.1

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/mkdep.gcc.sh ${DESTDIR}/usr/bin/mkdep

.include <bsd.prog.mk>
