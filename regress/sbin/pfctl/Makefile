# $OpenBSD: Makefile,v 1.65 2003/01/25 20:46:45 cedric Exp $

PFTESTS=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27
PFTESTS+=28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48
PFFAIL=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
PFSIMPLE=1 2
PFSETUP=1
PFLOAD=1 2 3 4 5 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 23 24 25 26 27 28 29
PFLOAD+=30 31 32 33 34 35 36 38 39 40 43 44 48
PFTABLE=1 2 3 4 5 6 7 8 9 10 11 12

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

.for n in ${PFLOAD}
REGRESS_TARGETS+=pfload${n}

pfload${n}:
	${SUDO} pfctl -a regress:regress -f - < ${.CURDIR}/pf${n}.in
	${SUDO} pfctl -a regress:regress -vvsr | \
	    diff -u ${.CURDIR}/pf${n}.loaded /dev/stdin
	${SUDO} pfctl -a regress:regress -Fn >/dev/null 2>&1
	${SUDO} pfctl -a regress:regress -Fr >/dev/null 2>&1

.endfor

.for n in ${PFTABLE}
REGRESS_TARGETS+=pfr${n}

pfr${n}:
	${SUDO} /bin/ksh ${.CURDIR}/pfr.exec ${.CURDIR}/pfr${n}.in | \
	    diff -u ${.CURDIR}/pfr${n}.ok /dev/stdin

.endfor


.ifdef DO_PFSETUP
.for n in ${PFSETUP}
REGRESS_TARGETS+=pfsetup${n}

pfsetup${n}:
	${SUDO} ${SHELL} ${.CURDIR}/pfsetup${n}.setup
	pfctl -nv -f - < ${.CURDIR}/pfsetup${n}.in | \
	    diff -u ${.CURDIR}/pfsetup${n}.ok /dev/stdin
	${SUDO} ${SHELL} ${.CURDIR}/pfsetup${n}.clean

.endfor
.endif

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
