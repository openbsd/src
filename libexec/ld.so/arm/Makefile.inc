#	$OpenBSD: Makefile.inc,v 1.6 2019/05/10 13:29:21 guenther Exp $

CFLAGS += -fpic -msoft-float
AFLAGS += -D_STANDALONE
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script
CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch/arm
SRCS+= divsi3.S
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.PATH: ${LIBCSRCDIR}/arch/arm/gen
