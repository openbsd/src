#	$OpenBSD: Makefile.inc,v 1.1 2002/08/21 15:40:30 art Exp $

CFLAGS += -fpic -mno-fp-regs
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.include "${LIBCSRCDIR}/arch/alpha/Makefile.inc"
