#	$OpenBSD: Makefile,v 1.19 2003/07/31 07:44:45 otto Exp $
#	$NetBSD: Makefile,v 1.17 1995/10/24 22:18:34 jtc Exp $

MAN=	znew.1

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	   ${.CURDIR}/znew \
	   ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
