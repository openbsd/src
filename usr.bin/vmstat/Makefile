#	$OpenBSD: Makefile,v 1.8 2007/12/15 03:43:41 deraadt Exp $

PROG=	vmstat

SRCS=	dkstats.c vmstat.c
MAN=	vmstat.8
DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>

