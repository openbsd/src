#	$OpenBSD: Makefile,v 1.6 2002/05/23 18:43:00 deraadt Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	snake
SRCS=	snake.c snscore.c
MAN=	snake.6
DPADD=	${LIBM} ${LIBCURSES}
LDADD=	-lm -lcurses
MLINKS=snake.6 snscore.6

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/games/snakerawscores
	(cd ${DESTDIR}/usr/games; rm -f snscore; ln -s dm snscore)

.include <bsd.prog.mk>
