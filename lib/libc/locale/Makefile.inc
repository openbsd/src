#	$OpenBSD: Makefile.inc,v 1.9 2005/07/24 09:50:49 espie Exp $

# locale sources
.PATH: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/locale ${LIBCSRCDIR}/locale

SRCS+=	_def_messages.c _def_monetary.c _def_numeric.c _def_time.c \
	localeconv.c nl_langinfo.c setlocale.c iswctype_sb.c mbrtowc_sb.c \
	multibyte_sb.c \
	__mb_cur_max.c wcstod.c wcstol.c wcstoul.c wcstoll.c wcstoull.c

MAN+=	nl_langinfo.3 setlocale.3 iswalnum.3 towlower.3 \
	btowc.3 mblen.3 mbrlen.3 mbrtowc.3 mbsinit.3 mbsrtowcs.3 \
	mbstowcs.3 mbtowc.3 wcrtomb.3 wcsrtombs.3 wcstombs.3 wctob.3 \
	wctomb.3 wctype.3 iswctype.3 wctrans.3 towctrans.3
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
