#	$OpenBSD: Makefile.inc,v 1.10 2025/05/08 13:57:23 deraadt Exp $

CFLAGS += -fpic -msoft-float
AFLAGS += -D_STANDALONE
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script
CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch/arm -I${.CURDIR}/../../lib/libc/arch
SRCS+= divsi3.S
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.PATH: ${LIBCSRCDIR}/arch/arm/gen
STACK_PROTECTOR=

# Suppress DWARF2 warnings
DEBUG?= -gdwarf-4

RELATIVE_RELOC=R_ARM_RELATIVE
