#	$NetBSD: Makefile,v 1.9 1996/03/30 06:49:21 cgd Exp $
#	@(#)Makefile	8.2 (Berkeley) 4/17/94

PROG=	savecore
.if (${MACHINE} == "atari")		# XXX until other ports use new libkvm
SRCS=	savecore.c
.else
SRCS=	savecore_old.c
.endif
SRCS+=	zopen.c
MAN=	savecore.8
.PATH:	${.CURDIR}/../../usr.bin/compress

.include <bsd.prog.mk>
