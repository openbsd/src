#	$OpenBSD: Makefile.inc,v 1.2 2005/10/23 06:04:03 drahn Exp $

CFLAGS += -fpic -msoft-float
AFLAGS += -D_STANDALONE
AFLAGS += -I${.CURDIR}/../../lib/libc/arch/arm
SRCS+= divsi3.S
ELF_LDFLAGS+=-z nocombreloc
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.PATH: ${LIBCSRCDIR}/arch/arm/gen/
