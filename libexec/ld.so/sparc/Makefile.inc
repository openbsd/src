#	$OpenBSD: Makefile.inc,v 1.2 2002/11/23 19:14:25 drahn Exp $

CFLAGS += -fpic -msoft-float -I${LIBCSRCDIR}/arch/sparc
AFLAGS = ${CFLAGS} -DSTRONG_SPARC
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.include "${LIBCSRCDIR}/arch/sparc/Makefile.inc"
.PATH: ${LIBCSRCDIR}/arch/sparc/gen/
SRCS+=umul.S mul.S
