#	$NetBSD: Makefile,v 1.6 1995/03/23 08:28:14 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	fish
MAN=	fish.6
HIDEGAME=hidegame

beforeinstall: 
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/fish.instr \
	    ${DESTDIR}/usr/share/games/fish.instr

.include <bsd.prog.mk>
