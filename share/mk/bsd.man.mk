#	$OpenBSD: bsd.man.mk,v 1.36 2011/07/05 04:49:35 schwarze Exp $
#	$NetBSD: bsd.man.mk,v 1.23 1996/02/10 07:49:33 jtc Exp $
#	@(#)bsd.man.mk	5.2 (Berkeley) 5/11/90

.if !target(.MAIN)
.  if exists(${.CURDIR}/../Makefile.inc)
.    include "${.CURDIR}/../Makefile.inc"
.  endif

.MAIN: all
.endif

# Add / so that we don't have to specify it.
.if defined(MANSUBDIR) && !empty(MANSUBDIR)
MANSUBDIR:=${MANSUBDIR:S,^,/,:S,$,/,}
.else
MANSUBDIR=/
.endif

CLEANFILES+=.man-linted *.manlint

.for page in ${MAN}
${page}.manlint: ${page}
	mandoc -Tlint -Wfatal ${.ALLSRC}
	@touch ${.TARGET}

all: ${page}.manlint

.  for sub in ${MANSUBDIR}
_MAN_INST=${DESTDIR}${MANDIR}${page:E}${sub}${page:T}
${_MAN_INST}: ${page}
	${INSTALL} ${INSTALL_COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} \
		${.ALLSRC} ${.TARGET}

maninstall: ${_MAN_INST}
.  endfor
.endfor

maninstall:
.if defined(MLINKS) && !empty(MLINKS)
.  for sub in ${MANSUBDIR}
.     for lnk file in ${MLINKS}
	@l=${DESTDIR}${MANDIR}${lnk:E}${sub}${lnk}; \
	t=${DESTDIR}${MANDIR}${file:E}${sub}${file}; \
	echo $$t -\> $$l; \
	rm -f $$t; ln $$l $$t;
.     endfor
.  endfor
.endif

BEFOREMAN?=
all: ${BEFOREMAN} ${MAN}
