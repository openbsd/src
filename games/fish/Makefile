#	$OpenBSD: Makefile,v 1.4 1997/09/21 11:35:54 deraadt Exp $

PROG=	fish
MAN=	fish.6
HIDEGAME=hidegame

beforeinstall: 
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/fish.instr ${DESTDIR}/usr/share/games/fish.instr

.include <bsd.prog.mk>
