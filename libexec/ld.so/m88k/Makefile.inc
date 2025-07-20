#	$OpenBSD: Makefile.inc,v 1.6 2025/07/20 16:09:07 miod Exp $

CFLAGS += -fpic

# Uncomment the following section if changes in the code cause large struct
# assignments to cause the compiler to emit memcpy() calls.

#LIBCSRCDIR=${.CURDIR}/../../lib/libc
#.PATH: ${LIBCSRCDIR}/arch/m88k/string
#CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch
#SRCS+= memcpy.S

LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script
STACK_PROTECTOR=

RELATIVE_RELOC=R_88K_BBASED_32
