#	$OpenBSD: Makefile,v 1.1 2005/08/03 22:33:58 espie Exp $

.include <bsd.own.mk>
LIB=    stubs
SRCS=	setlocale.c db.c
NOPIC=	Yes
NOPROFILE=Yes
NOMAN=1

CFLAGS+=-Os -D__DBINTERFACE_PRIVATE

LIBCSRCDIR=${.CURDIR}/../../../lib/libc

.include <bsd.lib.mk>
