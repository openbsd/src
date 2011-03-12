#	$OpenBSD: Makefile.inc,v 1.3 2011/03/12 18:53:07 deraadt Exp $

CFLAGS += -fpic -msoft-float
AFLAGS += -D_STANDALONE
AFLAGS += -I${.CURDIR}/../../lib/libc/arch/arm
SRCS+= divsi3.S
ELF_LDFLAGS+=-z nocombreloc
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.PATH: ${LIBCSRCDIR}/arch/arm/gen
