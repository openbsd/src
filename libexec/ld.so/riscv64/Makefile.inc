#	$OpenBSD: Makefile.inc,v 1.1 2021/04/28 15:16:26 drahn Exp $

CFLAGS += -fpic
CFLAGS += -march=rv64gc		# this prevents the use of float in ld.so
AFLAGS += -D_STANDALONE
CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch/riscv64
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

# Suppress DWARF2 warnings
DEBUG?= -gdwarf-4

RELATIVE_RELOC=R_RISCV_RELATIVE
