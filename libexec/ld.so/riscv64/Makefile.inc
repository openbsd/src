#	$OpenBSD: Makefile.inc,v 1.2 2021/06/26 14:47:54 kettenis Exp $

CFLAGS += -fpic
CFLAGS += -march=rv64imac	# this prevents the use of float in ld.so
AFLAGS += -D_STANDALONE
AFLAGS += -march=rv64imac
CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch/riscv64
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

# Suppress DWARF2 warnings
DEBUG?= -gdwarf-4

RELATIVE_RELOC=R_RISCV_RELATIVE
