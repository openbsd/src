# $OpenBSD: Makefile,v 1.8 2001/09/06 18:45:13 jasoni Exp $

NOMAN=
NOPROG=
PFTESTS=1 2 3 4 5 6 7 8 9

pfail1:
	@pfctl -nv -R - < ${.CURDIR}/pfail1.in > /dev/null 2>&1 || \
	    echo 'test pfail1 fails as expected'

.for n in ${PFTESTS}
regress: pf${n}

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

regress: pfail1 binat1

.PHONY: regress pfail1
.include <bsd.prog.mk>
