#	$NetBSD: Makefile,v 1.3 1994/12/22 06:17:44 jtc Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	locate

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/updatedb.csh ${DESTDIR}/usr/libexec/locate.updatedb

.include "../../Makefile.inc"
.include <bsd.prog.mk>
