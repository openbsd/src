# $OpenBSD: Makefile,v 1.18 2002/06/11 02:16:30 kjell Exp $

PFTESTS=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
PFFAIL=1 2 3 4 5 6 7 8 9 10

.for n in ${PFFAIL}
REGRESSTARGETS+=pfail${n}

pfail${n}:
	if pfctl -nv -f - < ${.CURDIR}/pfail${n}.in > /dev/null 2>&1 ; then \
		false ; \
	fi

.endfor

.for n in ${PFTESTS}
REGRESSTARGETS+=pf${n}

pf${n}:
	pfctl -nv -f - < ${.CURDIR}/pf${n}.in | \
	    cmp -s ${.CURDIR}/pf${n}.ok /dev/stdin

.endfor

.PHONY: ${REGRESSTARGETS}

.include <bsd.regress.mk>
