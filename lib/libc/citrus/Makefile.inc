#	$OpenBSD: Makefile.inc,v 1.2 2016/09/05 09:47:02 schwarze Exp $

# citrus sources
.PATH: ${LIBCSRCDIR}/citrus

SRCS+=	citrus_none.c citrus_utf8.c
CFLAGS+=-I${.CURDIR}
