#	$OpenBSD: Makefile,v 1.2 1996/06/26 05:35:51 deraadt Exp $
#	$NetBSD: Makefile,v 1.3 1994/12/22 06:17:44 jtc Exp $

PROG=	locate

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/updatedb.csh ${DESTDIR}/usr/libexec/locate.updatedb

.include "../../Makefile.inc"
.include <bsd.prog.mk>
