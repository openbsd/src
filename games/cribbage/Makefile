#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:35:46 deraadt Exp $

PROG=	cribbage
DPADD=	${LIBCURSES} ${LIBTERMLIB}
LDADD=	-lcurses -ltermlib
SRCS=	extern.c crib.c cards.c instr.c io.c score.c support.c
MAN=	cribbage.6
HIDEGAME=hidegame

beforeinstall: 
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/cribbage.n ${DESTDIR}/usr/share/games/cribbage.instr

.include <bsd.prog.mk>
