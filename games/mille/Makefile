#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:36:17 deraadt Exp $

PROG=	mille
SRCS=	comp.c end.c extern.c init.c mille.c misc.c move.c print.c \
	roll.c save.c types.c varpush.c
DPADD=	${LIBCURSES} ${LIBTERMLIB} ${LIBCOMPAT}
LDADD=	-lcurses -ltermlib -lcompat
MAN=	mille.6
HIDEGAME=hidegame

.include <bsd.prog.mk>

