# $OpenBSD: Makefile,v 1.2 2014/03/28 23:25:54 schwarze Exp $

REGRESS_TARGETS = center numbers span vert
SKIP_TMAN ?= ALL

TBL = /usr/local/bin/tbl

.for t in ${REGRESS_TARGETS}
${t}.out_ascii: ${t}.in
	${TBL} ${.ALLSRC} | ${NROFF} > ${.TARGET}
.endfor

.include <bsd.regress.mk>
