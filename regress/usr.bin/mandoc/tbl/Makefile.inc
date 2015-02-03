# $OpenBSD: Makefile.inc,v 1.2 2015/02/03 19:37:25 schwarze Exp $

TBL = /usr/local/bin/tbl

SKIP_TMAN ?= ALL

.for t in ${REGRESS_TARGETS}
${t}.out_ascii: ${t}.in
	${TBL} ${.ALLSRC} | ${NROFF} ${NOPTS} -Tascii > ${.TARGET}
.endfor

.include "../Makefile.inc"
