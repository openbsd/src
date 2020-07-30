# $OpenBSD: Makefile.inc,v 1.29 2020/07/30 21:32:19 schwarze Exp $

.include "Makefile.sub"

# --- programs ---

DIFF	 = diff -au
MAN	?= man
MANDOC	?= mandoc
MOPTS	?= -mandoc

# --- lists of targets ---

SKIP_ASCII ?=
SKIP_TMAN ?=
SKIP_MARKDOWN ?=

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
.  if empty(SKIP_MARKDOWN:MALL) && empty(SKIP_MARKDOWN:M${t})
_MARKDOWNFILES += ${t}.mandoc_markdown
_MARKDOWNDIFFS += ${t}.diff_markdown
.  endif
.endfor

_TAGFILES = ${TAG_TARGETS:C/$/.mandoc_tag/}
_TAGDIFFS = ${TAG_TARGETS:C/$/.diff_tag/}

_UTF8FILES = ${UTF8_TARGETS:C/$/.mandoc_utf8/}
_UTF8DIFFS = ${UTF8_TARGETS:C/$/.diff_utf8/}

_HTMLFILES = ${HTML_TARGETS:C/$/.mandoc_html/}
_HTMLDIFFS = ${HTML_TARGETS:C/$/.diff_html/}

_LINTFILES = ${LINT_TARGETS:C/$/.mandoc_lint/}
_LINTDIFFS = ${LINT_TARGETS:C/$/.diff_lint/}

_HTMLVALS = ${REGRESS_TARGETS:C/$/.validate_html/}

# --- convenience targets ---

# Note that bsd.regress.mk automatically adds REGRESS_TARGETS to all.
all: clean

ascii: ascii-clean ${_ASCIIDIFFS} ${_TAGDIFFS}

utf8: utf8-clean ${_UTF8DIFFS}

html: html-clean ${_HTMLDIFFS}

markdown: markdown-clean ${_MARKDOWNDIFFS}

tman: tman-clean ${_MANDIFFS}

lint: lint-clean ${_LINTDIFFS}

htmlval: ${_HTMLVALS}

# --- suffix rules ---

.SUFFIXES:	.diff_ascii .diff_html .diff_lint \
		.diff_man .diff_markdown .diff_tag .diff_utf8 \
		.in .in_man \
		.mandoc_ascii .mandoc_html .mandoc_lint \
		.mandoc_man .mandoc_markdown .mandoc_tag .mandoc_utf8 \
		.out_ascii .out_lint .out_utf8

.in.mandoc_ascii:
	${MANDOC} ${MOPTS} -Ios=OpenBSD -Tascii ${.IMPSRC} > ${.TARGET}

.mandoc_ascii.diff_ascii:
	${DIFF} ${.CURDIR}/${.IMPSRC:S/mandoc_ascii$/out_ascii/} ${.IMPSRC}

.in.mandoc_tag:
	${MAN} -l ${MOPTS} -Ios=OpenBSD -Tascii \
	    -O outfilename=${.TARGET:S/tag$/ascii/},tagfilename=${.TARGET} \
	    ${.IMPSRC}

.mandoc_tag.diff_tag:
	${DIFF} ${.CURDIR}/${.IMPSRC:S/mandoc_tag$/out_tag/} ${.IMPSRC}
	${DIFF} ${.CURDIR}/${.IMPSRC:S/mandoc_tag$/out_ascii/} \
	    ${.IMPSRC:S/mandoc_tag$/mandoc_ascii/}

.in.mandoc_utf8:
	${MANDOC} ${MOPTS} -Ios=OpenBSD -Tutf8 ${.IMPSRC} > ${.TARGET}

.mandoc_utf8.diff_utf8:
	${DIFF} ${.CURDIR}/${.IMPSRC:S/mandoc_utf8$/out_utf8/} ${.IMPSRC}

.in.mandoc_html:
	${MANDOC} ${MOPTS} -Thtml ${.IMPSRC} | \
		sed -n '/BEGINTEST/,/ENDTEST/p' | sed '1d;$$d' > ${.TARGET}

.mandoc_html.diff_html:
	${DIFF} ${.CURDIR}/${.IMPSRC:S/mandoc_html$/out_html/} ${.IMPSRC}

.in.mandoc_markdown:
	${MANDOC} ${MOPTS} -Ios=OpenBSD -Tmarkdown ${.IMPSRC} > ${.TARGET}

.mandoc_markdown.diff_markdown:
	${DIFF} ${.CURDIR}/${.IMPSRC:S/mandoc_markdown$/out_markdown/} \
		${.IMPSRC}

.in.in_man:
	${MANDOC} ${MOPTS} -Ios=OpenBSD -Tman ${.IMPSRC} > ${.TARGET}

.in_man.mandoc_man:
	${MANDOC} -man -Tascii -Omdoc ${.IMPSRC} > ${.TARGET}

.in.mandoc_lint:
	-${MANDOC} ${MOPTS} -Ios=OpenBSD -Tlint -Wall ${.IMPSRC} 2>&1 | \
		sed -E 's/^mandoc: \/[^:]+\//mandoc: /' > ${.TARGET}

.mandoc_lint.diff_lint:
	${DIFF} ${.CURDIR}/${.IMPSRC:S/mandoc_lint$/out_lint/} ${.IMPSRC}

# --- single-file targets ---

.for t in ${REGRESS_TARGETS}
.  if empty(SKIP_ASCII:MALL) && empty(SKIP_ASCII:M${t})
.    if empty(SKIP_TMAN:MALL) && empty(SKIP_TMAN:M${t})
${t}.diff_man: ${t}.mandoc_man
	${DIFF} ${.CURDIR}/${t}.out_ascii ${.ALLSRC}
.    else
${t}.diff_man:
.    endif
.    if empty(TAG_TARGETS:M${t})
${t}: ${t}.diff_ascii ${t}.diff_man
.    else
${t}: ${t}.diff_tag ${t}.diff_man
.    endif
.  else
${t}:
.  endif
${t}.validate_html: ${t}.in
	-${MANDOC} ${MOPTS} -Thtml ${.ALLSRC} | validate
.endfor

.for t in ${TAG_TARGETS}
${t}: ${t}.diff_tag
.endfor

.for t in ${UTF8_TARGETS}
${t}: ${t}.diff_utf8
.endfor

.for t in ${HTML_TARGETS}
${t}: ${t}.diff_html
.endfor

.for t in ${REGRESS_TARGETS}
.  if empty(SKIP_MARKDOWN:MALL) && empty(SKIP_MARKDOWN:M${t})
${t}: ${t}.diff_markdown
.  endif
.endfor

.for t in ${LINT_TARGETS}
${t}: ${t}.diff_lint
.endfor

# --- phony targets ---

clean: ${_CLEAN_TARGETS}

ascii-clean:
.if !empty(_ASCIIFILES)
	rm -f ${_ASCIIFILES} ${_TAGFILES}
.endif

utf8-clean:
.if !empty(_UTF8FILES)
	rm -f ${_UTF8FILES}
.endif

html-clean:
.if !empty(_HTMLFILES)
	rm -f ${_HTMLFILES}
.endif

markdown-clean:
.if !empty(_MARKDOWNFILES)
	rm -f ${_MARKDOWNFILES}
.endif

tman-clean:
.if !empty(_MANFILES)
	rm -f ${_MANFILES}
.endif

lint-clean:
.if !empty(_LINTFILES)
	rm -f ${_LINTFILES}
.endif

.PHONY: ${_ASCIIDIFFS} ${_TAGDIFFS} ${_UTF8DIFFS} ${_HTMLDIFFS} \
	${_MARKDOWNDIFFS} ${_MANDIFFS} ${_LINTDIFFS} ${_HTMLVALS}


# ----------------------------------------------------------------------
# development targets, may affect version-controlled files
# ----------------------------------------------------------------------

# --- the groff program ---

GROFF	 = /usr/local/bin/groff ${GOPTS} -mtty-char ${MOPTS} -Wall -P-c

# --- the list of targets ---

SKIP_GROFF_ASCII += ${SKIP_GROFF}
SKIP_GROFF_UTF8  += ${SKIP_GROFF}

_OUTFILES =
_HTMLOUTFILES =
_MARKDOWNOUTFILES =
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

.for t in ${REGRESS_TARGETS}
.  if empty(SKIP_MARKDOWN:MALL) && empty(SKIP_MARKDOWN:M${t})
_MARKDOWNOUTFILES += ${t}.out_markdown
.  endif
.endfor

.for t in ${LINT_TARGETS}
_LINTOUTFILES += ${t}.out_lint
.endfor

# --- the convenience targets ---

groff: obj-check ${_OUTFILES}

html-out: obj-check ${_HTMLOUTFILES}

markdown-out: obj-check ${_MARKDOWNOUTFILES}

lint-out: obj-check ${_LINTOUTFILES}

# --- single-file targets ---

.for t in ${REGRESS_TARGETS}
.  if empty(SKIP_ASCII:M${t}) && empty(SKIP_ASCII:MALL)
.    if empty(SKIP_GROFF_ASCII:M${t})
${t}.out_ascii: ${t}.in
	${GROFF} -Tascii ${.ALLSRC} > ${.TARGET}
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
	${GROFF} -Tutf8 ${.ALLSRC} > ${.TARGET}
.  else
.PHONY: ${t}.out_utf8
${t}.out_utf8:
	@echo "TODO: groff disabled for: ${.TARGET}"
.  endif
.endfor

.for t in ${HTML_TARGETS}
${t}.out_html: ${t}.in
	${MANDOC} ${MOPTS} -Thtml ${.ALLSRC} | \
		sed -n '/BEGINTEST/,/ENDTEST/p' | sed '1d;$$d' > ${.TARGET}
.endfor

.for t in ${REGRESS_TARGETS}
.  if empty(SKIP_MARKDOWN:MALL) && empty(SKIP_MARKDOWN:M${t})
${t}.out_markdown: ${t}.in
	${MANDOC} ${MOPTS} -Ios=OpenBSD -Tmarkdown ${.ALLSRC} > ${.TARGET}
.  endif
.endfor

.for t in ${LINT_TARGETS}
${t}.out_lint: ${t}.in
	-${MANDOC} ${MOPTS} -Ios=OpenBSD -Tlint -Wall ${.ALLSRC} \
		> ${.TARGET} 2>&1
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

markdown-out-clean: obj-check
.if !empty(_MARKDOWNOUTFILES)
	rm -f ${_MARKDOWNOUTFILES}
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
