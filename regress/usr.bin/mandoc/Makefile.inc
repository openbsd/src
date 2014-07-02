# $OpenBSD: Makefile.inc,v 1.12 2014/07/02 06:35:26 schwarze Exp $

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

_LINTFILES =
_LINTDIFFS =
.for t in ${LINT_TARGETS}
_LINTFILES += ${t}.mandoc_lint
_LINTDIFFS += ${t}.diff_lint
.endfor

# --- convenience targets ---

# Note that bsd.regress.mk automatically adds REGRESS_TARGETS to all.
all: clean

ascii: ascii-clean ${_ASCIIDIFFS}

utf8: utf8-clean ${_UTF8DIFFS}

tman: tman-clean ${_MANDIFFS}

lint: lint-clean ${_LINTDIFFS}

# --- suffix rules ---

.SUFFIXES:	.diff_ascii .diff_lint .diff_man .diff_utf8 .in .in_man \
		.mandoc_ascii .mandoc_lint .mandoc_man .mandoc_utf8 \
		.out_ascii .out_lint .out_utf8

.in.mandoc_ascii:
	@echo "${MANDOC} -Tascii ${.IMPSRC:T} > ${.TARGET}"
	@${MANDOC} -Tascii ${.IMPSRC} > ${.TARGET}

.mandoc_ascii.diff_ascii:
	@echo "${DIFF} ${.IMPSRC:S/mandoc_ascii$/out_ascii/} ${.IMPSRC}"
	@${DIFF} ${.CURDIR}/${.IMPSRC:S/mandoc_ascii$/out_ascii/} ${.IMPSRC}

.in.mandoc_utf8:
	@echo "${MANDOC} -Tutf8 ${.IMPSRC:T} > ${.TARGET}"
	@${MANDOC} -Tutf8 ${.IMPSRC} > ${.TARGET}

.mandoc_utf8.diff_utf8:
	@echo "${DIFF} ${.IMPSRC:S/mandoc_utf8$/out_utf8/} ${.IMPSRC}"
	@${DIFF} ${.CURDIR}/${.IMPSRC:S/mandoc_utf8$/out_utf8/} ${.IMPSRC}

.in.in_man:
	@echo "${MANDOC} -Tman ${.IMPSRC:T} > ${.TARGET}"
	@${MANDOC} -Tman ${.IMPSRC} > ${.TARGET}

.in_man.mandoc_man:
	${MANDOC} -Tascii -Omdoc ${.IMPSRC} > ${.TARGET}

.in.mandoc_lint:
	@echo "${MANDOC} -Tlint ${.IMPSRC:T} > ${.TARGET} 2>&1"
	@-${MANDOC} -Tlint ${.IMPSRC} 2>&1 | \
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
.endfor

.for t in ${UTF8_TARGETS}
${t}: ${t}.diff_utf8
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

tman-clean:
.if !empty(_MANFILES)
	rm -f ${_MANFILES}
.endif

lint-clean:
.if !empty(_LINTFILES)
	rm -f ${_LINTFILES}
.endif

.PHONY: ${_ASCIIDIFFS} ${_LINTDIFFS} ${_MANDIFFS} ${_UTF8DIFFS}


# ----------------------------------------------------------------------
# development targets, may affect version-controlled files
# ----------------------------------------------------------------------

# --- the groff program ---

NROFF = /usr/local/bin/nroff -c -mandoc

# --- the list of targets ---

SKIP_GROFF ?=

_OUTFILES =
_LINTOUTFILES =
.for t in ${REGRESS_TARGETS}
.  if empty(SKIP_GROFF:M${t}) && \
      empty(SKIP_ASCII:M${t}) && empty(SKIP_ASCII:MALL)
_OUTFILES += ${t}.out_ascii
.  endif
.endfor

.for t in ${UTF8_TARGETS}
.  if empty(SKIP_GROFF:M${t})
_OUTFILES += ${t}.out_utf8
.  endif
.endfor

.for t in ${LINT_TARGETS}
_LINTOUTFILES += ${t}.out_lint
.endfor

# --- the convenience targets ---

groff: obj-check ${_OUTFILES}

lint-out: obj-check ${_LINTOUTFILES}

# --- single-file targets ---

.for t in ${REGRESS_TARGETS}
.  if empty(SKIP_ASCII:M${t}) && empty(SKIP_ASCII:MALL)
.    if empty(SKIP_GROFF:M${t})
${t}.out_ascii: ${t}.in
	${NROFF} -Tascii ${.ALLSRC} > ${.TARGET}
.    else
.PHONY: ${t}.out_ascii
${t}.out_ascii:
	@echo "TODO: groff disabled for: ${.TARGET}"
.    endif
.  endif
.endfor

.for t in ${UTF8_TARGETS}
.  if empty(SKIP_GROFF:M${t})
${t}.out_utf8: ${t}.in
	${NROFF} -Tutf8 ${.ALLSRC} > ${.TARGET}
.  else
.PHONY: ${t}.out_utf8
${t}.out_utf8:
	@echo "TODO: groff disabled for: ${.TARGET}"
.  endif
.endfor

.for t in ${LINT_TARGETS}
${t}.out_lint: ${t}.in
	-${MANDOC} -Tlint ${.ALLSRC} > ${.TARGET} 2>&1
.endfor

# --- phony targets ---

groff-clean: obj-check
.if !empty(_OUTFILES)
	rm -f ${_OUTFILES}
.endif

lint-out-clean: obj-check
.if !empty(_LINTOUTFILES)
	rm -f ${_LINTOUTFILES}
.endif

obj-clean:
	rm -rf ${.CURDIR}/obj/*
	rm -f ${.CURDIR}/obj

obj-check:
	@test ! -e ${.CURDIR}/obj || \
		(echo '*** obj exists, run make obj-clean first'; false)
