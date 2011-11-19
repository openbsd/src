#	$OpenBSD: Makefile.inc,v 1.3 2011/11/19 14:59:48 naddy Exp $

CFLAGS += -fPIC -mno-fp-regs -mbuild-constants
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.include "${LIBCSRCDIR}/arch/alpha/Makefile.inc"
