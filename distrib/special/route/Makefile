#	$OpenBSD: Makefile,v 1.3 2017/06/12 18:49:33 espie Exp $

PROG=	route
MAN=	route.8
SRCS=	route.c show.c

CFLAGS+=	-Wall -DSMALL

route.o .depend tags: keywords.h

.PATH:  ${.CURDIR}/../../../sbin/route
.include <bsd.prog.mk>
