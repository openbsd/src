#	$OpenBSD: Makefile,v 1.1 2014/12/23 17:16:03 deraadt Exp $

PROG=	route
MAN=	route.8
SRCS=	route.c show.c

CFLAGS+=	-Wall

route.o .depend lint tags: keywords.h

.PATH:  ${.CURDIR}/../../../sbin/route
.include <bsd.prog.mk>
