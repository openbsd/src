#	$OpenBSD: Makefile,v 1.4 1997/09/21 11:37:20 deraadt Exp $

PROG=	wump
MAN=	wump.6
HIDEGAME=hidegame

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/wump.info ${DESTDIR}/usr/share/games

.include <bsd.prog.mk>
