# $OpenBSD: Makefile,v 1.1 2012/05/27 02:01:38 schwarze Exp $

REGRESS_TARGETS=center numbers span
SKIP_TMAN ?= ALL

TBL=/usr/local/bin/tbl

.for t in ${REGRESS_TARGETS}
${t}.out_ascii: ${t}.in
	${TBL} ${.ALLSRC} | ${NROFF} > ${.TARGET}
.endfor

.include <bsd.regress.mk>
