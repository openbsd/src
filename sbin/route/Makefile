#	$OpenBSD: Makefile,v 1.5 1997/12/12 09:04:32 deraadt Exp $

PROG=	route
MAN=	route.8
SRCS=	route.c show.c keywords.c ccitt_addr.c
BINOWN=	root
BINMODE=555

route.o .depend lint tags: keywords.h

.include <bsd.prog.mk>
