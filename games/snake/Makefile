#	$OpenBSD: Makefile,v 1.7 2002/05/24 07:59:51 wilfried Exp $
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
	(cd ${DESTDIR}/usr/games; rm -f snscore; ln -s snake snscore)

.include <bsd.prog.mk>
