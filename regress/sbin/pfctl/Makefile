# $OpenBSD: Makefile,v 1.4 2001/08/23 04:29:57 deraadt Exp $

NOMAN=
NOPROG=
PFTESTS=1 2 3 4 5 6

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

regress: pfail1

.PHONY: regress pfail1
.include <bsd.prog.mk>
