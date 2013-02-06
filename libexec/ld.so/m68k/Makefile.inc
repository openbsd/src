#	$OpenBSD: Makefile.inc,v 1.2 2013/02/06 20:12:55 miod Exp $

CFLAGS += -fpic
CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch/m68k
LIBCSRCDIR=${.CURDIR}/../../lib/libc

SRCS+=	memcpy.c
