# $OpenBSD: Makefile,v 1.12 2002/01/01 23:05:30 art Exp $

PFTESTS=1 2 3 4 5 6 7 8 9 10 11 12 13
PFFAIL=1 2 3 4 5 6 7 8

.for n in ${PFFAIL}
REGRESSTARGETS+=pfail${n}

pfail${n}:
	@pfctl -nv -R - < ${.CURDIR}/pfail${n}.in > /dev/null 2>&1 && \
	    echo 'test pfail${n} does not fail as expected' || true

.PHONY: pfail${n} regress
.endfor

.for n in ${PFTESTS}
REGRESSTARGETS+=pf${n}

pf${n}: pf${n}.out
	@cmp -s ${.CURDIR}/pf${n}.ok pf${n}.out || \
	    echo 'test pf${n} output does not match expected output'
pf${n}.out:
	pfctl -nv -R - < ${.CURDIR}/pf${n}.in > $@

.PHONY: pf${n} regress
CLEANFILES+=pf${n}.out
.endfor

binat1: binat1.out
	@cmp -s ${.CURDIR}/binat1.ok binat1.out || \
	   echo 'test binat1 output does not match expected output'

binat1.out:
	pfctl -nv -N - < ${.CURDIR}/binat1.in > $@

CLEANFILES+=binat1.out

REGRESSTARGETS+=binat1

.include <bsd.regress.mk>
