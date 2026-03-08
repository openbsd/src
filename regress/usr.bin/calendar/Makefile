#	$OpenBSD: Makefile,v 1.5 2026/03/08 17:53:36 bluhm Exp $

FILES?!=	ls ${.CURDIR}/[0-9]*.out

CALENDAR?=	calendar
IN=		${.CURDIR}/in
OUT=		${.CURDIR}/${*}.out

.for f in ${FILES}
REGRESS_TARGETS+=${f:C|.*/||}
.endfor
.for f in ${FILES}
${f:C|.*/||}:
	@${CALENDAR} -f ${IN} -t ${f:C|.*/||:C/\..*//} | diff -u ${OUT} -
.endfor

REGRESS_TARGETS+=before after noweekend allusers
REGRESS_ROOT_TARGETS=allusers

before:
	${CALENDAR} -f ${.CURDIR}/beforeafter.in -t 20190109 -B 5 | diff -U 7 ${.CURDIR}/before.out -

after:
	${CALENDAR} -f ${.CURDIR}/beforeafter.in -t 20190111 -A 5 | diff -U 7 ${.CURDIR}/after.out -

noweekend:
	${CALENDAR} -f ${.CURDIR}/beforeafter.in -t 20190111 -A 0 | diff -U 7 ${.CURDIR}/noweekend.out -

allusers:
	${SUDO} ${CALENDAR} -a

.PHONY:	${REGRESS_TARGETS}

.include <bsd.regress.mk>
