#	$NetBSD: Makefile,v 1.3 1995/04/22 07:42:31 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	tetris
SRCS=	input.c screen.c shapes.c scores.c tetris.c
MAN=	tetris.6
DPADD=	${LIBTERM}
LDADD=	-ltermcap
HIDEGAME=hidegame

.include <bsd.prog.mk>
