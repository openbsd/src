#	$OpenBSD: Makefile,v 1.15 2017/06/12 18:49:34 espie Exp $

PROG=	route
MAN=	route.8
SRCS=	route.c show.c

CFLAGS+=	-Wall -DBFD

route.o .depend tags: keywords.h

.include <bsd.prog.mk>
