#	$OpenBSD: Makefile.inc,v 1.1 2013/01/20 23:01:44 miod Exp $

LIBCSRCDIR=${.CURDIR}/../../lib/libc
CFLAGS += -fpic
CPPFLAGS += -I${LIBCSRCDIR}/arch/m88k

SRCS+=	memcpy.c
