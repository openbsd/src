#	$OpenBSD: Makefile,v 1.7 2005/03/30 05:20:18 henning Exp $

PROG=	route
MAN=	route.8
SRCS=	route.c show.c keywords.c
BINOWN=	root
BINMODE=555

CPPFLAGS+=	-DINET6

route.o .depend lint tags: keywords.h

.include <bsd.prog.mk>
