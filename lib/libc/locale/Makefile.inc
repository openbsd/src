#	$OpenBSD: Makefile.inc,v 1.5 2005/04/14 20:59:10 espie Exp $

# locale sources
.PATH: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/locale ${LIBCSRCDIR}/locale

SRCS+=	_def_messages.c _def_monetary.c _def_numeric.c _def_time.c \
	localeconv.c nl_langinfo.c setlocale.c

MAN+=	nl_langinfo.3 setlocale.3
MLINKS+=setlocale.3 localeconv.3

CFLAGS+=-I${.CURDIR}
