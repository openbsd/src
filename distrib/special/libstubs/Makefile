#	$OpenBSD: Makefile,v 1.2 2009/04/17 03:47:16 deraadt Exp $

.include <bsd.own.mk>
LIB=    stubs
SRCS=	setlocale.c db.c sha2.c
NOPIC=	Yes
NOPROFILE=Yes
NOMAN=1

CFLAGS+=-Os -D__DBINTERFACE_PRIVATE -DSHA256_ONLY

LIBCSRCDIR=${.CURDIR}/../../../lib/libc

.PATH: ${.CURDIR}/../../../lib/libc/hash

.include <bsd.lib.mk>
