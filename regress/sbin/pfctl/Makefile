# $OpenBSD: Makefile,v 1.52 2002/12/06 14:29:55 dhartmei Exp $

PFTESTS=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27
PFTESTS+=28 29 30 31 32 33 34 35 36 37 38 39 40
PFFAIL=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
PFSIMPLE=1 2
PFSETUP=1

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

REGRESS_TARGETS+=selfpf${n}
selfpf${n}:
	pfctl -nv -f - < ${.CURDIR}/pf${n}.ok | \
	    diff -u ${.CURDIR}/pf${n}.ok /dev/stdin

.endfor

.for n in ${PFSIMPLE}
REGRESS_TARGETS+=pfsimple${n}

pfsimple${n}:
	pfctl -nf - < ${.CURDIR}/pfsimple${n}.in

.endfor

.ifdef DO_PFSETUP
.for n in ${PFSETUP}
REGRESS_TARGETS+=pfsetup${n}

pfsetup${n}:
	${SHELL} ${.CURDIR}/pfsetup${n}.setup
	pfctl -nv -f - < ${.CURDIR}/pfsetup${n}.in | \
	    diff -u ${.CURDIR}/pfsetup${n}.ok /dev/stdin
	${SHELL} ${.CURDIR}/pfsetup${n}.clean

.endfor
.endif

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
