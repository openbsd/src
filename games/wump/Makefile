#	$NetBSD: Makefile,v 1.4 1995/04/22 08:19:45 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	wump
MAN=	wump.6
HIDEGAME=hidegame

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/wump.info \
	    ${DESTDIR}/usr/share/games

.include <bsd.prog.mk>
