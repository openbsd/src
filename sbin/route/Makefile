#	$OpenBSD: Makefile,v 1.8 2005/03/30 05:34:30 henning Exp $

PROG=	route
MAN=	route.8
SRCS=	route.c show.c keywords.c
BINOWN=	root
BINMODE=555

route.o .depend lint tags: keywords.h

.include <bsd.prog.mk>
