# $OpenBSD: Makefile.inc,v 1.1 2015/01/29 23:34:45 schwarze Exp $

TBL = /usr/local/bin/tbl

SKIP_TMAN ?= ALL SILENT

.for t in ${REGRESS_TARGETS}
${t}.out_ascii: ${t}.in
	${TBL} ${.ALLSRC} | ${NROFF} ${NOPTS} -Tascii > ${.TARGET}
.endfor

.include "../Makefile.inc"
