#	$OpenBSD: Makefile.inc,v 1.9 2022/05/24 17:15:22 guenther Exp $

CFLAGS += -fpic -msoft-float
AFLAGS += -D_STANDALONE
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script
CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch/arm -I${.CURDIR}/../../lib/libc/arch
SRCS+= divsi3.S
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.PATH: ${LIBCSRCDIR}/arch/arm/gen

# Suppress DWARF2 warnings
DEBUG?= -gdwarf-4

RELATIVE_RELOC=R_ARM_RELATIVE
