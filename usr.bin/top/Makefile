#	$OpenBSD: Makefile,v 1.1 1997/08/14 14:00:20 downsj Exp $
#
# Makefile for OpenBSD top-3.4.

PROG=	top

CFLAGS+=-I. -I${.CURDIR}
SRCS=	commands.c display.c machine.c screen.c top.c username.c utils.c \
	version.c
DPADD=	${LIBTERMLIB} ${LIBM} ${LIBKVM}
LDADD=	-ltermlib -lm -lkvm
BINGRP=	kmem
BINMODE=2555

CLEANFILES+=	sigdesc.h

.depend commands.o: sigdesc.h

sigdesc.h:
	awk -f ${.CURDIR}/sigconv.awk /usr/include/sys/signal.h > sigdesc.h

.include <bsd.prog.mk>
