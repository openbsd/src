#	$OpenBSD: Makefile,v 1.4 1997/09/21 11:38:05 deraadt Exp $

PROG=	route
MAN=	route.8
SRCS=	route.c show.c keywords.c ccitt_addr.c
BINOWN=	root
BINMODE=4555

route.o .depend lint tags: keywords.h

.include <bsd.prog.mk>
