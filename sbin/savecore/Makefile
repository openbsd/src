#	$NetBSD: Makefile,v 1.11 1996/05/12 04:36:29 mhitch Exp $
#	@(#)Makefile	8.2 (Berkeley) 4/17/94

PROG=	savecore

# XXX Temporary, until other ports use new libkvm
.if	(${MACHINE} == "amiga") || \
	(${MACHINE} == "atari") || \
	(${MACHINE} == "sun3")
SRCS=	savecore.c
LDADD=	-lkvm
DPADD=	${LIBKVM}
.else
SRCS=	savecore_old.c
.endif
SRCS+=	zopen.c
MAN=	savecore.8
.PATH:	${.CURDIR}/../../usr.bin/compress

.include <bsd.prog.mk>
