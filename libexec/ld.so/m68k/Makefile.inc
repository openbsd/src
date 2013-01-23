#	$OpenBSD: Makefile.inc,v 1.1 2013/01/23 19:15:58 miod Exp $

CFLAGS += -fpic
CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch/m68k
LIBCSRCDIR=${.CURDIR}/../../lib/libc
