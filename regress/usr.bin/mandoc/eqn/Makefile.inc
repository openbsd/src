# $OpenBSD: Makefile.inc,v 1.1.1.1 2015/01/01 12:53:46 schwarze Exp $

EQN = /usr/local/bin/eqn

SKIP_GROFF ?= ${REGRESS_TARGETS}
SKIP_TMAN ?= ALL SILENT

_FULLHTMLFILES = ${HTML_TARGETS:S/$/.html/}

html-clean:
.if !empty(_FULLHTMLFILES)
	rm -f ${_HTMLFILES} ${_FULLHTMLFILES}
.endif

.for t in ${HTML_TARGETS}
${t}.out_html: ${t}.in
	${MANDOC} ${MOPTS} -Thtml ${.ALLSRC} | \
		${.CURDIR}/../extract.pl > ${.TARGET}
.endfor

.include "../Makefile.inc"

.in.mandoc_html:
	${MANDOC} ${MOPTS} -Thtml ${.IMPSRC} > ${.TARGET:S/mandoc_html$/html/}
	@echo "extract.pl < ${.TARGET:S/mandoc_html$/html/} > ${.TARGET}"
	@${.CURDIR}/../extract.pl < ${.TARGET:S/mandoc_html$/html/} > ${.TARGET}
