#	$OpenBSD: Makefile,v 1.8 1998/08/30 22:29:56 millert Exp $

PROG=	savecore

# XXX Temporary, until other ports use new libkvm
.if	(${MACHINE} == "alpha") || \
	(${MACHINE} == "amiga") || \
	(${MACHINE} == "atari") || \
	(${MACHINE} == "powerpc") || \
	(${MACHINE} == "mvme88k") || \
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
