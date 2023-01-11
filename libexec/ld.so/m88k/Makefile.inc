#	$OpenBSD: Makefile.inc,v 1.4 2023/01/11 16:31:46 miod Exp $

CFLAGS += -fpic
SRCS+=	memcpy.c

LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

RELATIVE_RELOC=R_88K_BBASED_32
