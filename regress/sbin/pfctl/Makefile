# $OpenBSD: Makefile,v 1.25 2002/07/20 19:52:50 deraadt Exp $

PFTESTS=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
PFFAIL=1 2 3 4 5 6 7 8 9 10 11 12

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
	    diff -u ${.CURDIR}/pf${n}.ok /dev/stdin

.endfor

.PHONY: ${REGRESSTARGETS}

.include <bsd.regress.mk>
