#	$OpenBSD: Makefile,v 1.14 2007/09/25 06:02:36 otto Exp $
#
# Makefile for OpenBSD top-3.4.

PROG=	top

CFLAGS+=-I. -Wall -g -Wmissing-prototypes
SRCS=	commands.c display.c machine.c screen.c top.c username.c utils.c
DPADD=	${LIBCURSES}
LDADD=	-lcurses

.include <bsd.prog.mk>

