#	$OpenBSD: Makefile.inc,v 1.8 2024/06/04 15:14:45 deraadt Exp $

CFLAGS += -fPIC -mno-sse2 -mno-sse -mno-3dnow -mno-mmx
CFLAGS +=-fret-clean
AFLAGS += -fpic
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

# Suppress DWARF2 warnings
DEBUG?= -gdwarf-4

RELATIVE_RELOC=R_X86_64_RELATIVE
