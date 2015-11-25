#	$OpenBSD: Makefile,v 1.11 2015/11/25 23:18:11 deraadt Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	snake
SRCS=	snake.c snscore.c
MAN=	snake.6
DPADD=	${LIBM} ${LIBCURSES}
LDADD=	-lm -lcurses

.include <bsd.prog.mk>
