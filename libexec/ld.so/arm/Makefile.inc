#	$OpenBSD: Makefile.inc,v 1.1 2004/02/07 06:00:49 drahn Exp $

CFLAGS += -fpic -msoft-float
AFLAGS += -D_STANDALONE
SRCS+= divsi3.S
ELF_LDFLAGS+=-z nocombreloc
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.PATH: ${LIBCSRCDIR}/arch/arm/gen/
