#	$OpenBSD: Makefile,v 1.4 2017/10/17 19:31:56 naddy Exp $

PROG=	route
MAN=	route.8
SRCS=	route.c show.c

CFLAGS+=	-Wall -DSMALL

route.o tags: keywords.h

.PATH:  ${.CURDIR}/../../../sbin/route
.include <bsd.prog.mk>
