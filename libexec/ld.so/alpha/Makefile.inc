#	$OpenBSD: Makefile.inc,v 1.2 2006/09/26 23:14:52 martin Exp $

CFLAGS += -fPIC -mno-fp-regs
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.include "${LIBCSRCDIR}/arch/alpha/Makefile.inc"
