#	$OpenBSD: Makefile,v 1.8 2001/06/27 06:16:49 art Exp $
#
# Makefile for OpenBSD top-3.4.

PROG=	top

CFLAGS+=-I. -I${.CURDIR} -DORDER
SRCS=	commands.c display.c machine.c screen.c top.c username.c utils.c \
	version.c
DPADD=	${LIBCURSES} ${LIBM}
LDADD=	-lcurses -lm

CLEANFILES+=	sigdesc.h

.depend commands.o: sigdesc.h

sigdesc.h:	sigconv.awk
	awk -f ${.CURDIR}/sigconv.awk /usr/include/sys/signal.h > sigdesc.h

.include <bsd.prog.mk>

