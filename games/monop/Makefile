#	$OpenBSD: Makefile,v 1.3 1996/12/08 15:36:17 downsj Exp $
#	$NetBSD: Makefile,v 1.12 1995/03/23 08:34:32 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	monop
SRCS=	monop.c cards.c execute.c getinp.c houses.c jail.c misc.c morg.c \
	print.c prop.c rent.c roll.c spec.c trade.c
MAN=	monop.6
DPADD+=	${LIBCOMPAT}
LDADD+=	-lcompat
HIDEGAME=hidegame
CLEANFILES+=initdeck cards.pck

monop: cards.pck

cards.pck: initdeck
	${.OBJDIR}/initdeck ${.CURDIR}/cards.inp

initdeck: initdeck.c
	${HOSTCC} ${CFLAGS} ${LDFLAGS} ${LDSTATIC} -o ${.TARGET} ${.CURDIR}/initdeck.c ${LDADD}

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 cards.pck \
	    ${DESTDIR}/usr/share/games

.include <bsd.prog.mk>
