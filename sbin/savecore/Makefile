#	$OpenBSD: Makefile,v 1.16 2000/04/15 20:35:57 mickey Exp $

PROG=	savecore

# XXX Temporarily until pmax is able to use libkvm
.if	(${MACHINE} == "pmax")
SRCS=	savecore_old.c
.else
SRCS=	savecore.c
LDADD=	-lkvm
DPADD=	${LIBKVM}
.endif
SRCS+=	zopen.c
MAN=	savecore.8
.PATH:	${.CURDIR}/../../usr.bin/compress

.include <bsd.prog.mk>
