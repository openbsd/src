#	$OpenBSD: Makefile,v 1.4 1996/06/23 14:32:38 deraadt Exp $
#	$NetBSD: Makefile,v 1.11 1996/05/12 04:36:29 mhitch Exp $

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
