#	$OpenBSD: Makefile,v 1.3 1996/12/14 17:23:51 deraadt Exp $
#	$NetBSD: Makefile,v 1.12 1995/04/19 09:08:28 cgd Exp $

PROG=	route
MAN=	route.8
SRCS=	route.c show.c keywords.c ccitt_addr.c
BINOWN=	root
BINMODE=4555

route.o .depend lint tags: keywords.h

.include <bsd.prog.mk>
