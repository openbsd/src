#	$OpenBSD: Makefile.inc,v 1.3 2017/08/27 21:59:52 deraadt Exp $

LIBCSRCDIR=${.CURDIR}/../../lib/libc
CPPFLAGS+= -I${LIBCSRCDIR}/arch/mips64
