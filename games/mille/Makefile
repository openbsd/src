#	$OpenBSD: Makefile,v 1.5 1998/07/24 00:10:34 millert Exp $

PROG=	mille
SRCS=	comp.c end.c extern.c init.c mille.c misc.c move.c print.c \
	roll.c save.c types.c varpush.c
DPADD=	${LIBCURSES}
LDADD=	-lcurses
MAN=	mille.6
HIDEGAME=hidegame

.include <bsd.prog.mk>
