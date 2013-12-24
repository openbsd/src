# $OpenBSD: Makefile.inc,v 1.10 2013/12/24 15:58:07 schwarze Exp $

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


# --- convenience targets ---

# Note that bsd.regress.mk automatically adds REGRESS_TARGETS to all.
all: ascii-clean utf8-clean tman-clean

ascii: ascii-clean ${_ASCIIDIFFS}

utf8: utf8-clean ${_UTF8DIFFS}

tman: tman-clean ${_MANDIFFS}


# --- suffix rules ---

.SUFFIXES:	.diff_ascii .diff_utf8 .diff_man .in .in_man \
		.mandoc_ascii .mandoc_utf8 .mandoc_man \
		.out_ascii .out_utf8

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


# --- phony targets ---

clean: ascii-clean utf8-clean tman-clean

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

.PHONY: ascii ascii-clean utf8 utf8-clean tman tman-clean \
	${_ASCIIDIFFS} ${_UTF8DIFFS} ${_MANDIFFS}


# ----------------------------------------------------------------------
# development targets, may affect version-controlled files
# ----------------------------------------------------------------------

# --- the groff program ---

NROFF = /usr/local/bin/nroff -c -mandoc


# --- the list of targets ---

SKIP_GROFF ?=

_OUTFILES =
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


# --- the convenience target ---

groff: ${_OUTFILES}
	@test ! -e ${.CURDIR}/obj || \
		(echo "*** obj exists, run make obj-clean"; exit 1)


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


# --- phony targets ---

groff-clean: obj-clean
	rm -f ${_OUTFILES}

obj-clean: clean
	rm -rf ${.CURDIR}/obj/*
	rm -f ${.CURDIR}/obj

.PHONY: groff groff-clean obj-clean

