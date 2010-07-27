#	$OpenBSD: Makefile.inc,v 1.16 2010/07/27 16:59:03 stsp Exp $

# locale sources
.PATH: ${LIBCSRCDIR}/arch/${MACHINE_CPU}/locale ${LIBCSRCDIR}/locale

SRCS+=	btowc.c _def_messages.c _def_monetary.c _def_numeric.c _def_time.c \
	localeconv.c nl_langinfo.c setlocale.c iswctype.c __mb_cur_max.c \
	mblen.c mbrlen.c mbstowcs.c mbtowc.c multibyte_citrus.c wcscoll.c \
	wcstombs.c wctob.c wctomb.c wcstof.c wcstod.c wcstold.c wcstol.c \
	wcstoul.c wcstoll.c wcstoull.c wcstoimax.c wcstoumax.c \
	setrunelocale.c runeglue.c rune.c runetable.c ___runetype_mb.c \
	_wctrans.c wcsxfrm.c

MAN+=	nl_langinfo.3 setlocale.3 iswalnum.3 towlower.3 \
	btowc.3 mblen.3 mbrlen.3 mbrtowc.3 mbsinit.3 mbsrtowcs.3 \
	mbstowcs.3 mbtowc.3 wcrtomb.3 wcsrtombs.3 wcstod.3 wcstol.3 \
	wcstombs.3 wctob.3 wctomb.3 \
	wctype.3 iswctype.3 wctrans.3 towctrans.3
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
	towlower.3 towupper.3 \
	wcstod.3 wcstof.3 \
	wcstod.3 wcstold.3 \
	wcstol.3 wcstoul.3 \
	wcstol.3 wcstoll.3 \
	wcstol.3 wcstoull.3 \
	wcstol.3 wcstoimax.3 \
	wcstol.3 wcstoumax.3

CFLAGS+=-I${.CURDIR} -I${LIBCSRCDIR}/citrus
