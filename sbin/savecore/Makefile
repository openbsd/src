#	$OpenBSD: Makefile,v 1.7 1998/04/25 08:31:12 downsj Exp $

PROG=	savecore

# XXX Temporary, until other ports use new libkvm
.if	(${MACHINE} == "amiga") || \
	(${MACHINE} == "atari") || \
	(${MACHINE} == "sparc") || \
	(${MACHINE} == "sun3") || \
	(${MACHINE} == "hp300")
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
