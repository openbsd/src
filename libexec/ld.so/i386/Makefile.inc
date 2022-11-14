#	$OpenBSD: Makefile.inc,v 1.3 2022/11/14 13:20:23 deraadt Exp $

CFLAGS += -fPIC
AFLAGS += -fpic
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

RELATIVE_RELOC=R_386_RELATIVE
