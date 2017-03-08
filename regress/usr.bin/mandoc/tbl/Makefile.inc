# $OpenBSD: Makefile.inc,v 1.3 2017/03/08 22:53:36 schwarze Exp $

TBL = /usr/local/bin/tbl

SKIP_TMAN ?= ALL
SKIP_MARKDOWN ?= ALL

.for t in ${REGRESS_TARGETS}
${t}.out_ascii: ${t}.in
	${TBL} ${.ALLSRC} | ${NROFF} ${NOPTS} -Tascii > ${.TARGET}
.endfor

.include "../Makefile.inc"
