#	$OpenBSD: Makefile,v 1.10 1998/09/11 04:08:59 millert Exp $

PROG=	savecore

# XXX Temporary, until i386 uses new libkvm
.if	(${MACHINE} == "i386")
SRCS=	savecore_old.c
.else
SRCS=	savecore.c
LDADD=	-lkvm
DPADD=	${LIBKVM}
.endif
SRCS+=	zopen.c nlist.c
CFLAGS+=-D_NLIST_DO_GZIP
LDADD+=	-lz
DPADD+=	${LIBZ}
MAN=	savecore.8
.PATH:	${.CURDIR}/../../usr.bin/compress
.PATH:	${.CURDIR}/../../lib/libc/gen

.include <bsd.prog.mk>
