#	$OpenBSD: Makefile,v 1.14 2017/01/19 23:18:29 phessler Exp $

PROG=	route
MAN=	route.8
SRCS=	route.c show.c

CFLAGS+=	-Wall -DBFD

route.o .depend lint tags: keywords.h

.include <bsd.prog.mk>
