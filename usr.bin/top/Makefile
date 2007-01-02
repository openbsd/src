#	$OpenBSD: Makefile,v 1.11 2007/01/02 19:09:24 otto Exp $
#
# Makefile for OpenBSD top-3.4.

PROG=	top

CFLAGS+=-I.
SRCS=	commands.c display.c machine.c screen.c top.c username.c utils.c \
	version.c
DPADD=	${LIBCURSES}
LDADD=	-lcurses

CLEANFILES+=	sigdesc.h

.depend commands.o: sigdesc.h

sigdesc.h:	sigconv.awk
	awk -f ${.CURDIR}/sigconv.awk /usr/include/sys/signal.h > sigdesc.h

.include <bsd.prog.mk>

