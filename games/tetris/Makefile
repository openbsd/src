#	$OpenBSD: Makefile,v 1.4 1998/07/24 00:10:44 millert Exp $

PROG=	tetris
SRCS=	input.c screen.c shapes.c scores.c tetris.c
MAN=	tetris.6
DPADD=	${LIBCURSES}
LDADD=	-lcurses
HIDEGAME=hidegame

.include <bsd.prog.mk>
