#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:43:17 deraadt Exp $

PROG=	identd
SRCS=	config.c identd.c netbsd.c parse.c proxy.c version.c
MAN=	identd.8

LDADD=	-lkvm
DPADD=	${LIBKVM}

.include <bsd.prog.mk>
