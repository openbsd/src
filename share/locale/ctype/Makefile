# $OpenBSD: Makefile,v 1.10 2013/07/01 00:36:12 fgsch Exp $
# $NetBSD: Makefile,v 1.20 2005/02/10 18:03:02 tnozaki Exp $

NOMAN=	# defined

# pull LOCALEDIR and other declarations
.include <bsd.own.mk>

# pull in LOCALECHARSETS
.include "${.CURDIR}/Makefile.inc"

LOCALESRC_ARMSCII-8 = hy_AM.ARMSCII-8
LOCALESRC_CP1251 = bg_BG.CP1251
LOCALESRC_CP866 = ru_RU.CP866
LOCALESRC_ISO8859-1 = en_US.ISO_8859-1
LOCALESRC_ISO8859-2 = en_US.ISO_8859-2
LOCALESRC_ISO8859-4 = en_US.ISO_8859-4
LOCALESRC_ISO8859-5 = ru_RU.ISO_8859-5
LOCALESRC_ISO8859-7 = el_GR.ISO8859-7
LOCALESRC_ISO8859-9 = tr_TR.ISO8859-9
LOCALESRC_ISO8859-13 = lt_LT.ISO8859-13
LOCALESRC_ISO8859-15 = en_US.DIS_8859-15
LOCALESRC_KOI8-R = ru_RU.KOI8-R
LOCALESRC_KOI8-U = uk_UA.KOI8-U
LOCALESRC_UTF-8 = en_US.UTF-8

all: ${LOCALECHARSETS:S/$/.out/g}
realall: ${LOCALECHARSETS:S/$/.out/g}

CLEANFILES+=	${LOCALECHARSETS:S/$/.out/g}

FILES=	${LOCALECHARSETS:S/$/.out/g}
.for locale in ${LOCALECHARSETS}
FILESDIR_${locale}.out=	${LOCALEDIR}/${locale}
FILESNAME_${locale}.out=	LC_CTYPE
.endfor

install:
.for locale in ${LOCALECHARSETS}
	install -o ${LOCALEOWN} -g ${LOCALEGRP} -m ${DIRMODE} -d \
	    ${DESTDIR}${LOCALEDIR}/${locale}
	install -o ${LOCALEOWN} -g ${LOCALEGRP} -m ${LOCALEMODE} \
	    ${locale}.out ${DESTDIR}${LOCALEDIR}/${locale}/LC_CTYPE
.endfor

.for locale in ${LOCALECHARSETS}
${locale}.out: ${LOCALESRC_${locale}}.src
	${CPP} -I${.CURDIR} < ${.CURDIR}/${LOCALESRC_${locale}}.src | \
	    sed -e '/^#/d' | mklocale -o ${.TARGET}
.endfor

.SUFFIXES: .src .out
.src.out:
	${CPP} -I${.CURDIR} < ${.IMPSRC} | sed -e '/^#/d' | \
            mklocale -o ${.TARGET}


.include <bsd.prog.mk>
