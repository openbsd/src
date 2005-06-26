#	$OpenBSD: Makefile,v 1.18 2005/06/26 18:21:25 otto Exp $

PROG=	savecore

SRCS=	savecore.c zopen.c
CFLAGS=-DSAVECORE
LDADD=	-lkvm
DPADD=	${LIBKVM}
MAN=	savecore.8
.PATH:	${.CURDIR}/../../usr.bin/compress

.include <bsd.prog.mk>
