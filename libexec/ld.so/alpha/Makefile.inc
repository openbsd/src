#	$OpenBSD: Makefile.inc,v 1.4 2019/10/20 03:44:49 guenther Exp $

CFLAGS += -fPIC -mno-fp-regs -mbuild-constants
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.include "${LIBCSRCDIR}/arch/alpha/Makefile.inc"

RELATIVE_RELOC=R_ALPHA_RELATIVE
