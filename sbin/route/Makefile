#	$OpenBSD: Makefile,v 1.6 1999/12/08 07:55:54 itojun Exp $

PROG=	route
MAN=	route.8
SRCS=	route.c show.c keywords.c ccitt_addr.c
BINOWN=	root
BINMODE=555

CPPFLAGS+=	-DINET6

route.o .depend lint tags: keywords.h

.include <bsd.prog.mk>
