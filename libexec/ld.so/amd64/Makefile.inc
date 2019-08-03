#	$OpenBSD: Makefile.inc,v 1.6 2019/08/03 19:36:56 guenther Exp $

CFLAGS += -fPIC -mno-sse2 -mno-sse -mno-3dnow -mno-mmx
AFLAGS += -fpic
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

# Suppress DWARF2 warnings
DEBUG?= -gdwarf-4
