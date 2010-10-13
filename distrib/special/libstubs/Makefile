#	$OpenBSD: Makefile,v 1.8 2010/10/13 16:11:26 deraadt Exp $

.include <bsd.own.mk>
LIB=    stubs
SRCS=	db.c setlocale.c sha1.c sha2.c getgrent.c getpwent.c \
	ethers.c getaddrinfo.c gethostnamadr.c getnetnamadr.c \
	mbrtowc_sb.c vfprintf.c
NOPIC=	Yes
NOPROFILE=Yes
NOMAN=	1

CFLAGS+=-D__DBINTERFACE_PRIVATE -DSHA256_ONLY -UYP -I${LIBCSRCDIR}/include \
	-DNO_LOG_BAD_DNS_RESPONSES -I${LIBCSRCDIR}/locale \
	-I${LIBCSRCDIR}/stdio -DFLOATING_POINT

LIBCSRCDIR=${.CURDIR}/../../../lib/libc

.PATH: ${.CURDIR}/../../../lib/libc/hash
.PATH: ${.CURDIR}/../../../lib/libc/gen
.PATH: ${.CURDIR}/../../../lib/libc/net
.PATH: ${.CURDIR}/../../../lib/libc/stdio

.include <bsd.lib.mk>
