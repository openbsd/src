#	$OpenBSD: Makefile,v 1.4 2009/10/28 07:36:49 deraadt Exp $

.include <bsd.own.mk>
LIB=    stubs
SRCS=	db.c setlocale.c sha2.c getgrent.c getpwent.c \
	ethers.c getaddrinfo.c gethostnamadr.c getnetnamadr.c
NOPIC=	Yes
NOPROFILE=Yes
NOMAN=	1

CFLAGS+=-D__DBINTERFACE_PRIVATE -DSHA256_ONLY -UYP -I${LIBCSRCDIR}/include

LIBCSRCDIR=${.CURDIR}/../../../lib/libc

.PATH: ${.CURDIR}/../../../lib/libc/hash
.PATH: ${.CURDIR}/../../../lib/libc/gen
.PATH: ${.CURDIR}/../../../lib/libc/net

.include <bsd.lib.mk>
