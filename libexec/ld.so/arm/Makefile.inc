#	$OpenBSD: Makefile.inc,v 1.7 2019/08/03 19:36:56 guenther Exp $

CFLAGS += -fpic -msoft-float
AFLAGS += -D_STANDALONE
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script
CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch/arm
SRCS+= divsi3.S
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.PATH: ${LIBCSRCDIR}/arch/arm/gen

# Suppress DWARF2 warnings
DEBUG?= -gdwarf-4
