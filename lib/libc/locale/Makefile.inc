#	$OpenBSD: Makefile.inc,v 1.4 1998/11/20 11:18:42 d Exp $

# locale sources
.PATH: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/locale ${LIBCSRCDIR}/locale

SRCS+=	_def_messages.c _def_monetary.c _def_numeric.c _def_time.c \
	localeconv.c nl_langinfo.c setlocale.c

MAN+=	nl_langinfo.3 setlocale.3
MLINKS+=setlocale.3 localeconv.3
