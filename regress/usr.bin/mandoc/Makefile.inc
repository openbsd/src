# $OpenBSD: Makefile.inc,v 1.8 2013/11/10 14:58:44 schwarze Exp $

# --- programs ---

DIFF = diff -au
MANDOC ?= mandoc


# --- lists of targets ---

ASCIIFILES = ${REGRESS_TARGETS:C/$/.mandoc_ascii/}
ASCIIDIFFS = ${REGRESS_TARGETS:C/$/.diff_ascii/}

MANFILES = ${REGRESS_TARGETS:C/$/.mandoc_man/} ${REGRESS_TARGETS:C/$/.in_man/}
MANDIFFS = ${REGRESS_TARGETS:C/$/.diff_man/}
SKIP_TMAN ?=


# --- convenience targets ---

all: ascii-clean tman-clean

ascii: ascii-clean ${ASCIIDIFFS}

tman: tman-clean ${MANDIFFS}


# --- suffix rules ---

.SUFFIXES:	.diff_ascii .diff_man .in .in_man \
		.mandoc_ascii .mandoc_man .out_ascii

.in: ${.PREFIX}.diff_ascii ${.PREFIX}.diff_man
	@:

.in.mandoc_ascii:
	@echo "${MANDOC} ${.IMPSRC:T} > ${.TARGET}"
	@${MANDOC} ${.IMPSRC} > ${.TARGET}

.mandoc_ascii.diff_ascii:
	@echo "${DIFF} ${.IMPSRC:S/mandoc_ascii$/out_ascii/} ${.IMPSRC}"
	@${DIFF} ${.CURDIR}/${.IMPSRC:S/mandoc_ascii$/out_ascii/} ${.IMPSRC}

.in.in_man:
	@echo "${MANDOC} -Tman ${.IMPSRC:T} > ${.TARGET}"
	@${MANDOC} -Tman ${.IMPSRC} > ${.TARGET}

.in_man.mandoc_man:
	${MANDOC} -Omdoc ${.IMPSRC} > ${.TARGET}


# --- single-file targets ---

.for t in ${REGRESS_TARGETS}
.  if empty(SKIP_TMAN:MALL) && empty(SKIP_TMAN:M${t})
${t}.diff_man: ${t}.mandoc_man
	@echo "${DIFF} ${t}.out_ascii ${.ALLSRC}"
	@${DIFF} ${.CURDIR}/${t}.out_ascii ${.ALLSRC}
.  else
${t}.diff_man:
.    if empty(SKIP_TMAN:MSILENT)
	@echo "TODO: -Tman disabled for: ${.TARGET}"
.    endif
.  endif
.endfor


# --- phony targets ---

clean: ascii-clean tman-clean

ascii-clean:
	rm -f ${ASCIIFILES}

tman-clean:
	rm -f ${MANFILES}

.PHONY: ascii ascii-clean tman tman-clean ${ASCIIDIFFS} ${MANDIFFS}


# ----------------------------------------------------------------------
# development targets, may affect version-controlled files
# ----------------------------------------------------------------------

# --- the groff program ---

NROFF = /usr/local/bin/nroff -c -mandoc -Tascii


# --- the list of targets ---

SKIP_GROFF ?=

OUTFILES =
.for t in ${REGRESS_TARGETS}
.  if empty(SKIP_GROFF:M${t})
OUTFILES += ${t}.out_ascii
.  endif
.endfor


# --- the convenience target ---

groff: ${REGRESS_TARGETS:C/$/.out_ascii/}
	@test ! -e ${.CURDIR}/obj || \
		(echo "*** obj exists, run make obj-clean"; exit 1)


# --- single-file targets ---

.for t in ${REGRESS_TARGETS}
.  if empty(SKIP_GROFF:M${t})
${t}.out_ascii: ${t}.in
	${NROFF} ${.ALLSRC} > ${.TARGET}
.  else
.PHONY: ${t}.out_ascii
${t}.out_ascii:
	@echo "TODO: groff disabled for: ${.TARGET}"
.  endif
.endfor


# --- phony targets ---

groff-clean: obj-clean
	rm -f ${OUTFILES}

obj-clean: clean
	rm -rf ${.CURDIR}/obj/*
	rm -f ${.CURDIR}/obj

.PHONY: groff groff-clean obj-clean

