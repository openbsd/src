#	$OpenBSD: Makefile,v 1.17 2018/09/22 16:50:35 millert Exp $
#
# Makefile for OpenBSD top-3.4.

PROG=	top

CFLAGS+=-I. -Wall -Wmissing-prototypes
SRCS=	commands.c display.c machine.c screen.c top.c utils.c
DPADD=	${LIBCURSES}
LDADD=	-lcurses

.include <bsd.prog.mk>
