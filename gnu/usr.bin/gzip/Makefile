#	$OpenBSD: Makefile,v 1.18 2003/07/29 11:54:33 otto Exp $
#	$NetBSD: Makefile,v 1.17 1995/10/24 22:18:34 jtc Exp $

MAN=	gzexe.1 znew.1

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	   ${.CURDIR}/gzexe ${.CURDIR}/znew \
	   ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
