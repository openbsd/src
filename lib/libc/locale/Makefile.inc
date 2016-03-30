#	$OpenBSD: Makefile.inc,v 1.22 2016/03/30 06:38:41 jmc Exp $

# locale sources
.PATH: ${LIBCSRCDIR}/locale

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
	wctype.3 iswctype.3 wctrans.3 towctrans.3 wcwidth.3

CFLAGS+=-I${.CURDIR} -I${LIBCSRCDIR}/citrus
