#	$OpenBSD: Makefile,v 1.6 2002/05/31 03:46:35 pjanzen Exp $

PROG=	sail
SRCS=	main.c pl_main.c pl_1.c pl_2.c pl_3.c pl_4.c pl_5.c pl_6.c pl_7.c \
	dr_main.c dr_1.c dr_2.c dr_3.c dr_4.c dr_5.c lo_main.c \
	assorted.c game.c globals.c misc.c parties.c sync.c version.c
MAN=	sail.6
DPADD=	${LIBCURSES}
LDADD=	-lcurses
BINMODE=2555

.include <bsd.prog.mk>
