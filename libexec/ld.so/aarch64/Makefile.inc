#	$OpenBSD: Makefile.inc,v 1.5 2019/08/03 19:36:55 guenther Exp $

CFLAGS += -fpic
CFLAGS += -march=armv8-a+nofp+nosimd # this is -msoft-float
AFLAGS += -D_STANDALONE
CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch/aarch64
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

# Suppress DWARF2 warnings
DEBUG?= -gdwarf-4
