#	$OpenBSD: Makefile,v 1.1 1999/12/09 15:10:49 itojun Exp $

PROG=	rtsold
SRCS=	rtsold.c rtsol.c if.c probe.c dump.c

CPPFLAGS+=-DINET6
LDADD+=	-lkvm
DPADD+=	${LIBKVM}

MAN=	rtsold.8
MLINKS=	rtsold.8 rtsol.8

.include <bsd.prog.mk>
