#	$OpenBSD: Makefile.inc,v 1.3 2025/05/08 13:57:23 deraadt Exp $

CFLAGS += -fpic
CFLAGS += -march=rv64imac	# this prevents the use of float in ld.so
AFLAGS += -D_STANDALONE
AFLAGS += -march=rv64imac
CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch/riscv64
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script
STACK_PROTECTOR=

# Suppress DWARF2 warnings
DEBUG?= -gdwarf-4

RELATIVE_RELOC=R_RISCV_RELATIVE
