#	$NetBSD: Makefile,v 1.6 1995/04/22 08:34:27 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	snake
SRCS=	snake.c move.c
MAN=	snake.6
DPADD=	${LIBM} ${LIBCURSES} ${LIBCOMPAT}
LDADD=	-lm -lcurses -lcompat
HIDEGAME=hidegame

.include "../../Makefile.inc"
.include <bsd.prog.mk>
