#	$OpenBSD: Makefile.inc,v 1.3 1997/07/25 16:53:15 mickey Exp $

# locale sources
.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/locale ${.CURDIR}/locale

SRCS+=	_def_messages.c _def_monetary.c _def_numeric.c _def_time.c \
	localeconv.c nl_langinfo.c setlocale.c

MAN+=	nl_langinfo.3 setlocale.3
MLINKS+=setlocale.3 localeconv.3
