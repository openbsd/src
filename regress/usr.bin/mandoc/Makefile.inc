# $OpenBSD: Makefile.inc,v 1.19 2014/12/30 08:52:55 schwarze Exp $

.include "Makefile.sub"

# --- programs ---

DIFF = diff -au
MANDOC ?= mandoc

# --- lists of targets ---

SKIP_ASCII ?=
SKIP_TMAN ?=

_ASCIIFILES =
_ASCIIDIFFS =
_MANFILES =
_MANDIFFS =
.for t in ${REGRESS_TARGETS}
.  if empty(SKIP_ASCII:MALL) && empty(SKIP_ASCII:M${t})
_ASCIIFILES += ${t}.mandoc_ascii
_ASCIIDIFFS += ${t}.diff_ascii
.    if empty(SKIP_TMAN:MALL) && empty(SKIP_TMAN:M${t})
_MANFILES += ${t}.in_man ${t}.mandoc_man
_MANDIFFS += ${t}.diff_man
.    endif
.  endif
.endfor

_UTF8FILES = ${UTF8_TARGETS:C/$/.mandoc_utf8/}
_UTF8DIFFS = ${UTF8_TARGETS:C/$/.diff_utf8/}

_HTMLFILES = ${HTML_TARGETS:C/$/.mandoc_html/}
_HTMLDIFFS = ${HTML_TARGETS:C/$/.diff_html/}

_LINTFILES =
_LINTDIFFS =
.for t in ${LINT_TARGETS}
_LINTFILES += ${t}.mandoc_lint
_LINTDIFFS += ${t}.diff_lint
.endfor

_HTMLVALS = ${REGRESS_TARGETS:C/$/.validate_html/}

# --- convenience targets ---

# Note that bsd.regress.mk automatically adds REGRESS_TARGETS to all.
all: clean

ascii: ascii-clean ${_ASCIIDIFFS}

utf8: utf8-clean ${_UTF8DIFFS}

html: html-clean ${_HTMLDIFFS}

tman: tman-clean ${_MANDIFFS}

lint: lint-clean ${_LINTDIFFS}

htmlval: ${_HTMLVALS}

# --- suffix rules ---

.SUFFIXES:	.diff_ascii .diff_html .diff_lint .diff_man .diff_utf8 \
		.in .in_man \
		.mandoc_ascii .mandoc_html .mandoc_lint \
		.mandoc_man .mandoc_utf8 \
		.out_ascii .out_lint .out_utf8

.in.mandoc_ascii:
	@echo "${MANDOC} ${MOPTS} -Tascii ${.IMPSRC:T} > ${.TARGET}"
	@${MANDOC} ${MOPTS} -Tascii ${.IMPSRC} > ${.TARGET}

.mandoc_ascii.diff_ascii:
	@echo "${DIFF} ${.IMPSRC:S/mandoc_ascii$/out_ascii/} ${.IMPSRC}"
	@${DIFF} ${.CURDIR}/${.IMPSRC:S/mandoc_ascii$/out_ascii/} ${.IMPSRC}

.in.mandoc_utf8:
	@echo "${MANDOC} ${MOPTS} -Tutf8 ${.IMPSRC:T} > ${.TARGET}"
	@${MANDOC} ${MOPTS} -Tutf8 ${.IMPSRC} > ${.TARGET}

.mandoc_utf8.diff_utf8:
	@echo "${DIFF} ${.IMPSRC:S/mandoc_utf8$/out_utf8/} ${.IMPSRC}"
	@${DIFF} ${.CURDIR}/${.IMPSRC:S/mandoc_utf8$/out_utf8/} ${.IMPSRC}

.in.mandoc_html:
	@echo "${MANDOC} ${MOPTS} -Thtml ${.IMPSRC:T} > ${.TARGET}"
	@${MANDOC} ${MOPTS} -Thtml ${.IMPSRC} | \
		sed -n '/^BEGINTEST/,/^ENDTEST/p' > ${.TARGET}

.mandoc_html.diff_html:
	@echo "${DIFF} ${.IMPSRC:S/mandoc_html$/out_html/} ${.IMPSRC}"
	@${DIFF} ${.CURDIR}/${.IMPSRC:S/mandoc_html$/out_html/} ${.IMPSRC}

.in.in_man:
	@echo "${MANDOC} ${MOPTS} -Tman ${.IMPSRC:T} > ${.TARGET}"
	@${MANDOC} ${MOPTS} -Tman ${.IMPSRC} > ${.TARGET}

.in_man.mandoc_man:
	${MANDOC} -man -Tascii -Omdoc ${.IMPSRC} > ${.TARGET}

.in.mandoc_lint:
	@echo "${MANDOC} ${MOPTS} -Tlint ${.IMPSRC:T} > ${.TARGET} 2>&1"
	@-${MANDOC} ${MOPTS} -Tlint ${.IMPSRC} 2>&1 | \
		sed -E 's/^mandoc: \/[^:]+\//mandoc: /' > ${.TARGET}

.mandoc_lint.diff_lint:
	@echo "${DIFF} ${.IMPSRC:S/mandoc_lint$/out_lint/} ${.IMPSRC}"
	@${DIFF} ${.CURDIR}/${.IMPSRC:S/mandoc_lint$/out_lint/} ${.IMPSRC}

# --- single-file targets ---

.for t in ${REGRESS_TARGETS}
.  if empty(SKIP_ASCII:MALL) && empty(SKIP_ASCII:M${t})
.    if empty(SKIP_TMAN:MALL) && empty(SKIP_TMAN:M${t})
${t}.diff_man: ${t}.mandoc_man
	@echo "${DIFF} ${t}.out_ascii ${.ALLSRC}"
	@${DIFF} ${.CURDIR}/${t}.out_ascii ${.ALLSRC}
.    else
${t}.diff_man:
.      if empty(SKIP_TMAN:MSILENT)
	@echo "TODO: -Tman disabled for: ${.TARGET}"
.      endif
.    endif
${t}: ${t}.diff_ascii ${t}.diff_man
.  else
${t}:
	@echo "TODO: -Tascii disabled for: ${.TARGET}"
.  endif
${t}.validate_html: ${t}.in
	@echo "${MANDOC} ${MOPTS} -Thtml ${t}.in | validate"
	@-${MANDOC} ${MOPTS} -Thtml ${.ALLSRC} | validate
.endfor

.for t in ${UTF8_TARGETS}
${t}: ${t}.diff_utf8
.endfor

.for t in ${HTML_TARGETS}
${t}: ${t}.diff_html
.endfor

.for t in ${LINT_TARGETS}
${t}: ${t}.diff_lint
.endfor

# --- phony targets ---

clean: ${_CLEAN_TARGETS}

ascii-clean:
.if !empty(_ASCIIFILES)
	rm -f ${_ASCIIFILES}
.endif

utf8-clean:
.if !empty(_UTF8FILES)
	rm -f ${_UTF8FILES}
.endif

html-clean:
.if !empty(_HTMLFILES)
	rm -f ${_HTMLFILES}
.endif

tman-clean:
.if !empty(_MANFILES)
	rm -f ${_MANFILES}
.endif

lint-clean:
.if !empty(_LINTFILES)
	rm -f ${_LINTFILES}
.endif

.PHONY: ${_ASCIIDIFFS} ${_UTF8DIFFS} ${_HTMLDIFFS} ${_MANDIFFS} \
	${_LINTDIFFS} ${_HTMLVALS}


# ----------------------------------------------------------------------
# development targets, may affect version-controlled files
# ----------------------------------------------------------------------

# --- the groff program ---

NROFF	 = /usr/local/bin/nroff -c

.ifdef MOPTS
NOPTS	?= ${MOPTS}
.else
NOPTS	?= -mandoc -Wall
.endif

# --- the list of targets ---

SKIP_GROFF_ASCII += ${SKIP_GROFF}
SKIP_GROFF_UTF8  += ${SKIP_GROFF}

_OUTFILES =
_HTMLOUTFILES =
_LINTOUTFILES =
.for t in ${REGRESS_TARGETS}
.  if empty(SKIP_GROFF_ASCII:M${t}) && \
      empty(SKIP_ASCII:M${t}) && empty(SKIP_ASCII:MALL)
_OUTFILES += ${t}.out_ascii
.  endif
.endfor

.for t in ${UTF8_TARGETS}
.  if empty(SKIP_GROFF_UTF8:M${t})
_OUTFILES += ${t}.out_utf8
.  endif
.endfor

.for t in ${HTML_TARGETS}
_HTMLOUTFILES += ${t}.out_html
.endfor

.for t in ${LINT_TARGETS}
_LINTOUTFILES += ${t}.out_lint
.endfor

# --- the convenience targets ---

groff: obj-check ${_OUTFILES}

html-out: obj-check ${_HTMLOUTFILES}

lint-out: obj-check ${_LINTOUTFILES}

# --- single-file targets ---

.for t in ${REGRESS_TARGETS}
.  if empty(SKIP_ASCII:M${t}) && empty(SKIP_ASCII:MALL)
.    if empty(SKIP_GROFF_ASCII:M${t})
${t}.out_ascii: ${t}.in
	${NROFF} ${NOPTS} -Tascii ${.ALLSRC} > ${.TARGET}
.    else
.PHONY: ${t}.out_ascii
${t}.out_ascii:
	@echo "TODO: groff disabled for: ${.TARGET}"
.    endif
.  endif
.endfor

.for t in ${UTF8_TARGETS}
.  if empty(SKIP_GROFF_UTF8:M${t})
${t}.out_utf8: ${t}.in
	${NROFF} ${NOPTS} -Tutf8 ${.ALLSRC} > ${.TARGET}
.  else
.PHONY: ${t}.out_utf8
${t}.out_utf8:
	@echo "TODO: groff disabled for: ${.TARGET}"
.  endif
.endfor

.for t in ${HTML_TARGETS}
${t}.out_html: ${t}.in
	${MANDOC} ${MOPTS} -Thtml ${.ALLSRC} | \
		sed -n '/^BEGINTEST/,/^ENDTEST/p' > ${.TARGET}
.endfor

.for t in ${LINT_TARGETS}
${t}.out_lint: ${t}.in
	-${MANDOC} ${MOPTS} -Tlint ${.ALLSRC} > ${.TARGET} 2>&1
.endfor

# --- phony targets ---

groff-clean: obj-check
.if !empty(_OUTFILES)
	rm -f ${_OUTFILES}
.endif

html-out-clean: obj-check
.if !empty(_HTMLOUTFILES)
	rm -f ${_HTMLOUTFILES}
.endif

lint-out-clean: obj-check
.if !empty(_LINTOUTFILES)
	rm -f ${_LINTOUTFILES}
.endif

obj-clean:
	rm -rf ${.CURDIR}/obj/*
	rm -f ${.CURDIR}/obj
	cd ${.CURDIR} && ${MAKE} clean

obj-check:
	@test ! -e ${.CURDIR}/obj || \
		(echo '*** obj exists, run make obj-clean first'; false)
