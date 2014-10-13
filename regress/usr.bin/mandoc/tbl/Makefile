# $OpenBSD: Makefile,v 1.4 2014/10/13 23:31:26 schwarze Exp $

REGRESS_TARGETS = center fonts numbers span vert
SKIP_TMAN ?= ALL

TBL = /usr/local/bin/tbl

.for t in ${REGRESS_TARGETS}
${t}.out_ascii: ${t}.in
	${TBL} ${.ALLSRC} | ${NROFF} ${NOPTS} -Tascii > ${.TARGET}
.endfor

.include <bsd.regress.mk>
