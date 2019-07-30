#	$OpenBSD: Makefile,v 1.16 2014/07/16 20:06:27 okan Exp $
#
# Makefile for OpenBSD top-3.4.

PROG=	top

CFLAGS+=-I. -Wall -Wmissing-prototypes
SRCS=	commands.c display.c machine.c screen.c top.c username.c utils.c
DPADD=	${LIBCURSES}
LDADD=	-lcurses

.include <bsd.prog.mk>
