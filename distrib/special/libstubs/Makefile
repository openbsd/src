#	$OpenBSD: Makefile,v 1.5 2009/11/18 07:43:22 guenther Exp $

.include <bsd.own.mk>
LIB=    stubs
SRCS=	db.c setlocale.c sha2.c getgrent.c getpwent.c \
	ethers.c getaddrinfo.c gethostnamadr.c getnetnamadr.c
NOPIC=	Yes
NOPROFILE=Yes
NOMAN=	1

CFLAGS+=-D__DBINTERFACE_PRIVATE -DSHA256_ONLY -UYP -I${LIBCSRCDIR}/include \
	-DNO_LOG_BAD_DNS_RESPONSES

LIBCSRCDIR=${.CURDIR}/../../../lib/libc

.PATH: ${.CURDIR}/../../../lib/libc/hash
.PATH: ${.CURDIR}/../../../lib/libc/gen
.PATH: ${.CURDIR}/../../../lib/libc/net

.include <bsd.lib.mk>
