#	$OpenBSD: Makefile.inc,v 1.6 2005/04/30 09:14:23 espie Exp $

# locale sources
.PATH: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/locale ${LIBCSRCDIR}/locale

SRCS+=	_def_messages.c _def_monetary.c _def_numeric.c _def_time.c \
	localeconv.c nl_langinfo.c setlocale.c iswctype_sb.c

MAN+=	nl_langinfo.3 setlocale.3 iswalnum.3 towlower.3
MLINKS+=setlocale.3 localeconv.3 \
	iswalnum.3 iswalpha.3 \
	iswalnum.3 iswblank.3 \
	iswalnum.3 iswcntrl.3 \
	iswalnum.3 iswdigit.3 \
	iswalnum.3 iswgraph.3 \
	iswalnum.3 iswlower.3 \
	iswalnum.3 iswprint.3 \
	iswalnum.3 iswpunct.3 \
	iswalnum.3 iswspace.3 \
	iswalnum.3 iswupper.3 \
	iswalnum.3 iswxdigit.3 \
	towlower.3 towupper.3

CFLAGS+=-I${.CURDIR}
