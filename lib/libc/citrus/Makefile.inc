#	$OpenBSD: Makefile.inc,v 1.1 2010/07/27 16:59:03 stsp Exp $

# citrus sources
.PATH: ${LIBCSRCDIR}/citrus

SRCS+=	citrus_ctype.c citrus_none.c citrus_utf8.c
CFLAGS+=-I${.CURDIR}
