#	$OpenBSD: Makefile,v 1.8 2002/05/31 03:46:35 pjanzen Exp $

PROG=	cribbage
DPADD=	${LIBCURSES}
LDADD=	-lcurses
SRCS=	extern.c crib.c cards.c instr.c io.c score.c support.c
MAN=	cribbage.6
BINMODE=2555

beforeinstall: 
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/cribbage.n ${DESTDIR}/usr/share/games/cribbage.instr

.include <bsd.prog.mk>
