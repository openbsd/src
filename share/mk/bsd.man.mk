#	$OpenBSD: bsd.man.mk,v 1.34 2011/06/24 14:17:41 naddy Exp $
#	$NetBSD: bsd.man.mk,v 1.23 1996/02/10 07:49:33 jtc Exp $
#	@(#)bsd.man.mk	5.2 (Berkeley) 5/11/90

.if !target(.MAIN)
.  if exists(${.CURDIR}/../Makefile.inc)
.    include "${.CURDIR}/../Makefile.inc"
.  endif

.MAIN: all
.endif

.if defined(MANSUBDIR)
# Add / so that we don't have to specify it. Better arch -> MANSUBDIR mapping
MANSUBDIR:=${MANSUBDIR:S,^,/,}
.else
# XXX MANSUBDIR must be non empty for the mlink loops to work
MANSUBDIR=''
.endif

CLEANFILES+= .man-linted

.if defined(MAN) && !empty(MAN)
.man-linted: ${MAN}
	mandoc -Tlint -Wfatal ${.ALLSRC}
	@touch ${.TARGET}

all: .man-linted
.endif

.for page in ${MAN}
.  for sub in ${MANSUBDIR}
${DESTDIR}${MANDIR}${page:E}${sub}/${page:T}: ${page}
	${INSTALL} ${INSTALL_COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} \
		${.ALLSRC} ${.TARGET}

maninstall: ${DESTDIR}${MANDIR}${page:E}${sub}/${page:T}
.  endfor
.endfor

maninstall:
.if defined(MLINKS) && !empty(MLINKS)
.  for sub in ${MANSUBDIR}
.     for lnk file in ${MLINKS}
	@l=${DESTDIR}${MANDIR}${lnk:E}${sub}/${lnk}; \
	t=${DESTDIR}${MANDIR}${file:E}${sub}/${file}; \
	echo $$t -\> $$l; \
	rm -f $$t; ln $$l $$t;
.     endfor
.  endfor
.endif

BEFOREMAN?=
all: ${BEFOREMAN} ${MAN}
