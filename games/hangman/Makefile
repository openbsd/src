#	$OpenBSD: Makefile,v 1.6 2008/04/01 21:05:50 miod Exp $

PROG=	hangman
SRCS=	endgame.c extern.c getguess.c getword.c ksyms.c main.c playgame.c \
	prdata.c prman.c prword.c setup.c
MAN=	hangman.6
DPADD=	${LIBCURSES}
LDADD=	-lcurses

.include <bsd.prog.mk>


