#	$OpenBSD: Makefile.inc,v 1.2 2014/07/05 17:05:08 miod Exp $

# CFLAGS += -fpic -msoft-float
LIBCSRCDIR=${.CURDIR}/../../lib/libc
CPPFLAGS += -I${LIBCSRCDIR}/arch/mips64
