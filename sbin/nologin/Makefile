#	$OpenBSD: Makefile,v 1.4 1996/12/08 14:37:24 downsj Exp $
#	$NetBSD: Makefile,v 1.4 1995/03/18 14:59:08 cgd Exp $

MAN=	nologin.8

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/nologin.sh ${DESTDIR}/sbin/nologin

.include <bsd.prog.mk>
