#	$OpenBSD: Makefile.inc,v 1.6 2025/05/08 13:57:23 deraadt Exp $

CFLAGS += -fPIC -mno-fp-regs -mbuild-constants
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.include "${LIBCSRCDIR}/arch/alpha/Makefile.inc"
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script
STACK_PROTECTOR=

RELATIVE_RELOC=R_ALPHA_RELATIVE
