# $OpenBSD: Makefile,v 1.2 2014/08/14 01:58:51 schwarze Exp $

REGRESS_TARGETS=vspace

TBL=/usr/local/bin/tbl

.for t in ${REGRESS_TARGETS}
${t}.out_ascii: ${t}.in
	${TBL} ${.ALLSRC} | ${NROFF} ${NOPTS} -Tascii > ${.TARGET}
.endfor

.include <bsd.regress.mk>
