#	$OpenBSD: Makefile,v 1.3 1996/12/08 14:01:45 downsj Exp $
#	$NetBSD: Makefile,v 1.4 1995/03/18 14:59:08 cgd Exp $

MAN=	nologin.8

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/nologin.sh ${DESTDIR}/sbin/nologin

.include <bsd.prog.mk>
