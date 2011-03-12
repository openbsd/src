#	$OpenBSD: Makefile.inc,v 1.3 2011/03/12 18:53:07 deraadt Exp $

CFLAGS += -fpic -msoft-float -I${LIBCSRCDIR}/arch/sparc
AFLAGS = ${CFLAGS} -DSTRONG_SPARC
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.include "${LIBCSRCDIR}/arch/sparc/Makefile.inc"
.PATH: ${LIBCSRCDIR}/arch/sparc/gen
SRCS+=umul.S mul.S
