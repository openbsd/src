# $OpenBSD: Makefile,v 1.14 2002/06/07 18:36:54 frantzen Exp $

PFTESTS=1 2 3 4 5 6 7 8 9 10 11 12 13 14
PFFAIL=1 2 3 4 5 6 7 8

.for n in ${PFFAIL}
REGRESSTARGETS+=pfail${n}

pfail${n}:
	if pfctl -nv -R - < ${.CURDIR}/pfail${n}.in > /dev/null 2>&1 ; then \
		false ; \
	fi

.endfor

.for n in ${PFTESTS}
REGRESSTARGETS+=pf${n}

pf${n}:
	pfctl -nv -R - < ${.CURDIR}/pf${n}.in | \
	    cmp -s ${.CURDIR}/pf${n}.ok /dev/stdin

.endfor

binat1:
	pfctl -nv -N - < ${.CURDIR}/binat1.in | \
	    cmp -s ${.CURDIR}/binat1.ok /dev/stdin

REGRESSTARGETS+=binat1

.PHONY: ${REGRESSTARGETS}

.include <bsd.regress.mk>
