#	$OpenBSD: Makefile,v 1.10 2003/06/13 21:52:24 deraadt Exp $
#
# Makefile for OpenBSD top-3.4.

PROG=	top

CFLAGS+=-I.
SRCS=	commands.c display.c machine.c screen.c top.c username.c utils.c \
	version.c
DPADD=	${LIBCURSES} ${LIBM}
LDADD=	-lcurses -lm

CLEANFILES+=	sigdesc.h

.depend commands.o: sigdesc.h

sigdesc.h:	sigconv.awk
	awk -f ${.CURDIR}/sigconv.awk /usr/include/sys/signal.h > sigdesc.h

.include <bsd.prog.mk>

