# $OpenBSD: Makefile,v 1.17 2002/06/10 02:10:39 kjell Exp $

PFTESTS=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
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

NATTESTS=binat1 nat1 rdr1

.for n in ${NATTESTS}
REGRESSTARGETS+=${n}

${n}:
	pfctl -nv -N - < ${.CURDIR}/${n}.in | \
	    cmp -s ${.CURDIR}/${n}.ok /dev/stdin

.endfor

.PHONY: ${REGRESSTARGETS}

.include <bsd.regress.mk>
