#	$Id: Makefile,v 1.3 1997/08/01 19:08:39 deraadt Exp $

PROG=	identd
SRCS=	identd.c openbsd.c parse.c proxy.c version.c
MAN=	identd.8

LDADD=	-lkvm
DPADD=	${LIBKVM}

.include <bsd.prog.mk>
