#	$OpenBSD: Makefile,v 1.13 2007/05/29 00:56:56 otto Exp $
#
# Makefile for OpenBSD top-3.4.

PROG=	top

CFLAGS+=-I. -Wall -g -Wmissing-prototypes
SRCS=	commands.c display.c machine.c screen.c top.c username.c utils.c \
	version.c
DPADD=	${LIBCURSES}
LDADD=	-lcurses

.include <bsd.prog.mk>

