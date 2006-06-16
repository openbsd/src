#	$OpenBSD: Makefile,v 1.19 2006/06/16 23:49:00 kettenis Exp $

PROG=	savecore

SRCS=	savecore.c zopen.c
CFLAGS+=-DSAVECORE
LDADD=	-lkvm
DPADD=	${LIBKVM}
MAN=	savecore.8
.PATH:	${.CURDIR}/../../usr.bin/compress

.include <bsd.prog.mk>
