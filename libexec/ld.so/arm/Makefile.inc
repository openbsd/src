#	$OpenBSD: Makefile.inc,v 1.5 2016/08/14 18:46:03 guenther Exp $

CFLAGS += -fpic -msoft-float
AFLAGS += -D_STANDALONE
CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch/arm
SRCS+= divsi3.S
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.PATH: ${LIBCSRCDIR}/arch/arm/gen
