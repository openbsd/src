#	$OpenBSD: Makefile,v 1.11 2010/09/04 08:06:09 blambert Exp $

PROG=	route
MAN=	route.8
SRCS=	route.c show.c

route.o .depend lint tags: keywords.h

keywords.h: keywords.sh
	/bin/sh ${.CURDIR}/keywords.sh

.include <bsd.prog.mk>
