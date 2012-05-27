# $OpenBSD: Makefile,v 1.1 2012/05/27 01:01:25 schwarze Exp $

REGRESS_TARGETS=vspace

TBL=/usr/local/bin/tbl

.for t in ${REGRESS_TARGETS}
${t}.out_ascii: ${t}.in
	${TBL} ${.ALLSRC} | ${NROFF} > ${.TARGET}
.endfor

.include <bsd.regress.mk>
