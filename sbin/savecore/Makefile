#	$OpenBSD: Makefile,v 1.13 1998/09/24 06:24:20 millert Exp $

PROG=	savecore

# XXX Temporarely until i386 and pmax are able to use libkvm
.if	(${MACHINE} == "i386" || ${MACHINE} == "pmax")
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
