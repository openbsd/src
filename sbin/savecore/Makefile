#	$OpenBSD: Makefile,v 1.9 1998/09/05 16:33:25 millert Exp $

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
SRCS+=	zopen.c nlist.c
CFLAGS+=-D_NLIST_DO_GZIP
LDADD+=	-lz
DPADD+=	${LIBZ}
MAN=	savecore.8
.PATH:	${.CURDIR}/../../usr.bin/compress
.PATH:	${.CURDIR}/../../lib/libc/gen

.include <bsd.prog.mk>
