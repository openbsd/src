#	$OpenBSD: Makefile,v 1.15 2010/03/18 12:28:09 otto Exp $
#
# Makefile for OpenBSD top-3.4.

PROG=	top

CFLAGS+=-I. -Wall -Wmissing-prototypes
SRCS=	commands.c display.c machine.c screen.c top.c username.c utils.c
DPADD=	${LIBCURSES}
LDADD=	-lcurses

.include <bsd.prog.mk>

