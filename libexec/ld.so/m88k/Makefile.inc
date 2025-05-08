#	$OpenBSD: Makefile.inc,v 1.5 2025/05/08 13:57:23 deraadt Exp $

CFLAGS += -fpic
SRCS+=	memcpy.c

LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script
STACK_PROTECTOR=

RELATIVE_RELOC=R_88K_BBASED_32
