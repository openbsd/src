#	$OpenBSD: Makefile,v 1.17 2001/06/29 17:08:14 miod Exp $

PROG=	savecore

SRCS=	savecore.c zopen.c
LDADD=	-lkvm
DPADD=	${LIBKVM}
MAN=	savecore.8
.PATH:	${.CURDIR}/../../usr.bin/compress

.include <bsd.prog.mk>
