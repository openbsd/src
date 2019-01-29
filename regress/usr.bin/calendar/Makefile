#	$OpenBSD: Makefile,v 1.3 2019/01/29 20:10:26 millert Exp $

FILES?!=	ls ${.CURDIR}/[0-9]*.out

CALENDAR?=	calendar
IN=		${.CURDIR}/in
OUT=		${.CURDIR}/${*}.out

.for f in ${FILES}
REGRESS_TARGETS+=${f:C|.*/||}
.endfor
.for f in ${FILES}
${f:C|.*/||}:
	@echo ${*}
	@${CALENDAR} -f ${IN} -t ${f:C|.*/||:C/\..*//} | diff -u ${OUT} -
.endfor

REGRESS_TARGETS+=before after

before:
	@echo ${*}
	@${CALENDAR} -f ${.CURDIR}/beforeafter.in -t 20190109 -B 5 | diff -U 7 ${.CURDIR}/before.out -

after:
	@echo ${*}
	@${CALENDAR} -f ${.CURDIR}/beforeafter.in -t 20190111 -A 5 | diff -U 7 ${.CURDIR}/after.out -

.PHONY:	${REGRESS_TARGETS}

.include <bsd.regress.mk>
