#	$OpenBSD: Makefile.inc,v 1.4 2012/09/02 16:32:52 jsg Exp $

CFLAGS += -fpic -msoft-float
AFLAGS += -D_STANDALONE
CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch/arm
SRCS+= divsi3.S
ELF_LDFLAGS+=-z nocombreloc
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.PATH: ${LIBCSRCDIR}/arch/arm/gen
