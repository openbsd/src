#	$OpenBSD: Makefile,v 1.12 1998/09/17 01:16:14 millert Exp $

PROG=	savecore

# XXX Temporarely until i386 and pmax are able to use libkvm
.if	(${MACHINE} == "i386" || ${MACHINE} == "pmax")
SRCS=	savecore_old.c
.else
SRCS=	savecore.c
LDADD=	-lkvm
DPADD=	${LIBKVM}
.endif
SRCS+=	zopen.c nlist.c
#CFLAGS+=-D_NLIST_DO_GZIP
#LDADD+=	-lz
#DPADD+=	${LIBZ}
MAN=	savecore.8
.PATH:	${.CURDIR}/../../usr.bin/compress
.PATH:	${.CURDIR}/../../lib/libc/gen

.include <bsd.prog.mk>
