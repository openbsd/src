# $OpenBSD: Makefile,v 1.3 2001/08/19 16:31:06 dhartmei Exp $

NOMAN=
NOPROG=
PFTESTS=1 2 3 4

pfail1:
	@pfctl -nvR- < ${.CURDIR}/pfail1.in > /dev/null 2>&1 || \
	    echo 'test pfail1 should fail'

.for n in ${PFTESTS}
regress: pf${n}

pf${n}: pf${n}.out
	@cmp -s ${.CURDIR}/pf${n}.ok pf${n}.out || \
	    echo 'test pf${n} output does not match expected output'
pf${n}.out:
	pfctl -nvR- < ${.CURDIR}/pf${n}.in > $@

.PHONY: pf${n} regress
CLEANFILES+=pf${n}.out
.endfor

regress: pfail1

.PHONY: regress pfail1
.include <bsd.prog.mk>
