#	$OpenBSD: Makefile,v 1.4 1998/03/19 11:41:53 pjanzen Exp $

PROG=	mille
SRCS=	comp.c end.c extern.c init.c mille.c misc.c move.c print.c \
	roll.c save.c types.c varpush.c
DPADD=	${LIBCURSES} ${LIBTERMLIB}
LDADD=	-lcurses -ltermlib
MAN=	mille.6
HIDEGAME=hidegame

.include <bsd.prog.mk>

