#	$OpenBSD: Makefile,v 1.10 2014/07/16 20:06:27 okan Exp $

PROG=	vmstat

CPPFLAGS+= -D_LIBKVM
SRCS=	dkstats.c vmstat.c
MAN=	vmstat.8
DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
