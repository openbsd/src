#	$OpenBSD: Makefile,v 1.7 2002/05/23 18:43:00 deraadt Exp $

PROG=	monop
SRCS=	monop.c cards.c execute.c getinp.c houses.c jail.c misc.c morg.c \
	print.c prop.c rent.c roll.c spec.c trade.c
MAN=	monop.6
CLEANFILES+=initdeck cards.pck

monop: cards.pck

cards.pck: initdeck
	${.OBJDIR}/initdeck ${.CURDIR}/cards.inp

initdeck: initdeck.c
	${HOSTCC} ${CFLAGS} ${LDFLAGS} ${LDSTATIC} -o ${.TARGET} ${.CURDIR}/initdeck.c ${LDADD}

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 cards.pck \
	    ${DESTDIR}/usr/share/games

.include <bsd.prog.mk>
