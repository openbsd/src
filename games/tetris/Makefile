#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:37:07 deraadt Exp $

PROG=	tetris
SRCS=	input.c screen.c shapes.c scores.c tetris.c
MAN=	tetris.6
DPADD=	${LIBTERMCAP}
LDADD=	-ltermcap
HIDEGAME=hidegame

.include <bsd.prog.mk>
