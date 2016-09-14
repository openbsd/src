#	$OpenBSD: Makefile,v 1.1 2016/09/14 16:04:36 millert Exp $

FILES?!=	ls ${.CURDIR}/*.out

CALENDAR?=	calendar
IN=		${.CURDIR}/in
OUT=		${.CURDIR}/${*}.out

.for f in ${FILES}
REGRESS_TARGETS+=${f:C|.*/||}
.endfor
.for f in ${FILES}
${f:C|.*/||}:
	@echo ${*}
	@${CALENDAR} -f ${IN} -t ${f:C|.*/||:C/\..*//} | diff ${OUT} -
.endfor

.PHONY:	${REGRESS_TARGETS}

.include <bsd.regress.mk>
