#	$Id: Makefile,v 1.2 1996/07/25 09:47:18 deraadt Exp $

PROG=	identd
SRCS=	config.c identd.c openbsd.c parse.c proxy.c version.c
MAN=	identd.8

LDADD=	-lkvm
DPADD=	${LIBKVM}

.include <bsd.prog.mk>
