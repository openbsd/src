#	$OpenBSD: Makefile.inc,v 1.2 1996/08/19 08:28:10 tholo Exp $

# locale sources
.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/locale ${.CURDIR}/locale

SRCS+=	_def_messages.c _def_monetary.c _def_numeric.c _def_time.c \
	localeconv.c nl_langinfo.c setlocale.c

MAN+=	nl_langinfo.3
