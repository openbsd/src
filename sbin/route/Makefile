#	$OpenBSD: Makefile,v 1.13 2013/07/19 14:41:46 bluhm Exp $

PROG=	route
MAN=	route.8
SRCS=	route.c show.c

CFLAGS+=	-Wall

route.o .depend lint tags: keywords.h

.include <bsd.prog.mk>
