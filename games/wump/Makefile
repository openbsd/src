#	$OpenBSD: Makefile,v 1.3 1997/04/27 20:55:49 millert Exp $
#	$NetBSD: Makefile,v 1.4 1995/04/22 08:19:45 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	wump
MAN=	wump.6
HIDEGAME=hidegame

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/wump.info ${DESTDIR}/usr/share/games

.include <bsd.prog.mk>
