#	$OpenBSD: Makefile,v 1.3 1999/03/13 02:08:09 pjanzen Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	snake
SRCS=	snake.c move.c snscore.c
MAN=	snake.6
DPADD=	${LIBM} ${LIBCURSES}
LDADD=	-lm -lcurses
HIDEGAME=hidegame

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/games/snakerawscores
	(cd ${DESTDIR}/usr/games; rm -f snscore; ln -s dm snscore)
	(cd ${DESTDIR}/usr/games/hide; rm -f snscore; ln -s snake snscore)

.include <bsd.prog.mk>
