#	$OpenBSD: Makefile,v 1.6 2026/10/13 06:55:15 deraadt Exp $

PROG=	ypbind
MAN=	ypbind.8
CFLAGS+=-DHEURISTIC

LDADD=-lrpcsvc
DPADD=${LIBRPCSVC}

.include <bsd.prog.mk>
