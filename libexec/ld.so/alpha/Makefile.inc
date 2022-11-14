#	$OpenBSD: Makefile.inc,v 1.5 2022/11/14 13:21:50 deraadt Exp $

CFLAGS += -fPIC -mno-fp-regs -mbuild-constants
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.include "${LIBCSRCDIR}/arch/alpha/Makefile.inc"
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

RELATIVE_RELOC=R_ALPHA_RELATIVE
