#	$OpenBSD: Makefile,v 1.14 1998/09/26 21:39:42 ryker Exp $

PROG=	savecore

# XXX Temporarily until i386, pmax and mac68k are able to use libkvm
.if	(${MACHINE} == "i386" || ${MACHINE} == "pmax" || ${MACHINE} == "mac68k")
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
