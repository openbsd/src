#	$OpenBSD: Makefile.inc,v 1.1 2002/08/21 15:40:30 art Exp $

CFLAGS += -fpic -msoft-float -I${LIBCSRCDIR}/arch/sparc
AFLAGS = ${CFLAGS}
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.include "${LIBCSRCDIR}/arch/sparc/Makefile.inc"
.PATH: ${LIBCSRCDIR}/arch/sparc/gen/
SRCS+=umul.S
