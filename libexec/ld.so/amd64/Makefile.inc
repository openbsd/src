#	$OpenBSD: Makefile.inc,v 1.7 2019/10/20 03:44:49 guenther Exp $

CFLAGS += -fPIC -mno-sse2 -mno-sse -mno-3dnow -mno-mmx
AFLAGS += -fpic
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

# Suppress DWARF2 warnings
DEBUG?= -gdwarf-4

RELATIVE_RELOC=R_X86_64_RELATIVE
