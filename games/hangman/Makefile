#	$OpenBSD: Makefile,v 1.5 2002/05/23 18:43:00 deraadt Exp $

PROG=	hangman
SRCS=	endgame.c extern.c getguess.c getword.c main.c playgame.c \
	prdata.c prman.c prword.c setup.c
MAN=	hangman.6
DPADD=	${LIBCURSES}
LDADD=	-lcurses

.include <bsd.prog.mk>


