# $OpenBSD: Makefile,v 1.2 2004/01/25 20:45:13 deraadt Exp $

PROG=	ndp
SRCS=	ndp.c gmt2local.c
MAN=	ndp.8 # ndp.4

CPPFLAGS+=-DINET6 -I${.CURDIR}

.include <bsd.prog.mk>
