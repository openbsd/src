#	$OpenBSD: Makefile,v 1.7 2002/05/31 03:46:35 pjanzen Exp $

PROG=	tetris
SRCS=	input.c screen.c shapes.c scores.c tetris.c
MAN=	tetris.6
DPADD=	${LIBCURSES}
LDADD=	-lcurses
BINMODE=2555

beforeinstall:
	@if [ ! -f ${DESTDIR}/var/games/tetris.scores ]; then \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 664 \
		/dev/null ${DESTDIR}/var/games/tetris.scores ; \
	else \
		true ; \
	fi

.include <bsd.prog.mk>
