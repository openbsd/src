#	$NetBSD: Makefile,v 1.4 1995/10/08 23:44:02 gwr Exp $

PROG=	installboot
MAN=	installboot.8
BINDIR=/usr/mdec
DPADD=	${LIBUTIL}
LDADD=	-lutil

# Need this to work in the miniroot

LDSTATIC=	-static

.include <bsd.prog.mk>
