# $OpenBSD: Makefile,v 1.35 2002/09/18 14:03:48 henning Exp $

PFTESTS=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27
PFTESTS+=28 29
PFFAIL=1 2 3 4 5 6 7 8 9 10 11 12 13 14
PFSIMPLE=1 2

.for n in ${PFFAIL}
REGRESS_TARGETS+=pfail${n}

pfail${n}:
	if pfctl -nv -f - < ${.CURDIR}/pfail${n}.in > /dev/null 2>&1 ; then \
		false ; \
	fi

.endfor

.for n in ${PFTESTS}
REGRESS_TARGETS+=pf${n}

pf${n}:
	pfctl -nv -f - < ${.CURDIR}/pf${n}.in | \
	    diff -u ${.CURDIR}/pf${n}.ok /dev/stdin

.endfor

.for n in ${PFSIMPLE}
REGRESS_TARGETS+=pfsimple${n}

pfsimple${n}:
	pfctl -nf - < ${.CURDIR}/pfsimple${n}.in

.endfor

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
