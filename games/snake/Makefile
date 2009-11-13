#	$OpenBSD: Makefile,v 1.9 2009/11/13 21:50:12 deraadt Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	snake
SRCS=	snake.c snscore.c
MAN=	snake.6
DPADD=	${LIBM} ${LIBCURSES}
LDADD=	-lm -lcurses
BINMODE=2555

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/games/snakerawscores

.include <bsd.prog.mk>
