# $OpenBSD: Makefile,v 1.1 1999/12/10 07:26:56 itojun Exp $

.PATH:	${.CURDIR}/../tcpdump

PROG=	ndp
SRCS=	ndp.c gmt2local.c
MAN=	ndp.8 # ndp.4

CPPFLAGS+=-DINET6
CPPFLAGS+=-I${.CURDIR} -I${.CURDIR}/../tcpdump

.include <bsd.prog.mk>
