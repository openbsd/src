#	$OpenBSD: Makefile,v 1.12 2007/02/23 13:31:45 millert Exp $
#
# Makefile for OpenBSD top-3.4.

PROG=	top

CFLAGS+=-I.
SRCS=	commands.c display.c machine.c screen.c top.c username.c utils.c \
	version.c
DPADD=	${LIBCURSES}
LDADD=	-lcurses

.include <bsd.prog.mk>

