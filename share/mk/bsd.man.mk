#	$OpenBSD: bsd.man.mk,v 1.37 2011/07/06 04:10:27 schwarze Exp $
#	$NetBSD: bsd.man.mk,v 1.23 1996/02/10 07:49:33 jtc Exp $
#	@(#)bsd.man.mk	5.2 (Berkeley) 5/11/90

.if !target(.MAIN)
.  if exists(${.CURDIR}/../Makefile.inc)
.    include "${.CURDIR}/../Makefile.inc"
.  endif

.MAIN: all
.endif

BEFOREMAN?=
MANLINT=${MAN:S/$/.manlint/}
CLEANFILES+=.man-linted ${MANLINT}

# Add / so that we don't have to specify it.
.if defined(MANSUBDIR) && !empty(MANSUBDIR)
MANSUBDIR:=${MANSUBDIR:S,^,/,:S,$,/,}
.else
MANSUBDIR=/
.endif

# Files contained in ${BEFOREMAN} must be built before generating any
# manual page source code.  However, static manual page files contained
# in the source tree must not appear as targets, or the ${.IMPSRC} in
# the .man.manlint suffix rule below will not find them in the .PATH.
.for page in ${MAN}
.  if target(${page})
${page}: ${BEFOREMAN}
.  endif
.endfor

# In any case, ${BEFOREMAN} must be finished before linting any manuals.
.if !empty(MANLINT)
${MANLINT}: ${BEFOREMAN}
.endif

# Set up the suffix rules for checking manuals.
_MAN_SUFFIXES=1 2 3 3p 4 5 6 7 8 9
.for s in ${_MAN_SUFFIXES}
.SUFFIXES: .${s} .${s}.manlint
.${s}.${s}.manlint:
	mandoc -Tlint -Wfatal ${.IMPSRC}
	@touch ${.TARGET}
.endfor

# Install the real manuals.
.for page in ${MAN}
.  for sub in ${MANSUBDIR}
_MAN_INST=${DESTDIR}${MANDIR}${page:E}${sub}${page:T}
${_MAN_INST}: ${page}
	${INSTALL} ${INSTALL_COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} \
		${.ALLSRC} ${.TARGET}

maninstall: ${_MAN_INST}
.  endfor
.endfor

# Install the manual hardlinks, if any.
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

# Explicitly list ${BEFOREMAN} to get it done even if ${MAN} is empty.
all: ${BEFOREMAN} ${MAN} ${MANLINT}
