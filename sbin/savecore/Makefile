#	$OpenBSD: Makefile,v 1.20 2016/09/03 13:05:08 tedu Exp $

PROG=	savecore

SRCS=	savecore.c zopen.c
CFLAGS+=-DSAVECORE
LDADD=	-lkvm
DPADD=	${LIBKVM}
MAN=	savecore.8

.include <bsd.prog.mk>
