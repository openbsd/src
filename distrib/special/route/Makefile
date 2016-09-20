#	$OpenBSD: Makefile,v 1.2 2016/09/20 10:39:18 phessler Exp $

PROG=	route
MAN=	route.8
SRCS=	route.c show.c

CFLAGS+=	-Wall -DSMALL

route.o .depend lint tags: keywords.h

.PATH:  ${.CURDIR}/../../../sbin/route
.include <bsd.prog.mk>
