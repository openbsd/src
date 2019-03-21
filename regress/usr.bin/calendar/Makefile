#	$OpenBSD: Makefile,v 1.4 2019/03/21 19:31:57 millert Exp $

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

REGRESS_TARGETS+=before after noweekend

before:
	@echo ${*}
	@${CALENDAR} -f ${.CURDIR}/beforeafter.in -t 20190109 -B 5 | diff -U 7 ${.CURDIR}/before.out -

after:
	@echo ${*}
	@${CALENDAR} -f ${.CURDIR}/beforeafter.in -t 20190111 -A 5 | diff -U 7 ${.CURDIR}/after.out -

noweekend:
	@echo ${*}
	@${CALENDAR} -f ${.CURDIR}/beforeafter.in -t 20190111 -A 0 | diff -U 7 ${.CURDIR}/noweekend.out -

.PHONY:	${REGRESS_TARGETS}

.include <bsd.regress.mk>
