# $OpenBSD: Makefile,v 1.3 2013/08/09 17:52:12 bluhm Exp $

PROG=	ndp
SRCS=	ndp.c gmt2local.c
MAN=	ndp.8

CPPFLAGS+=-DINET6 -I${.CURDIR}

.include <bsd.prog.mk>
