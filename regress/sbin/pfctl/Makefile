# $OpenBSD: Makefile,v 1.98 2003/02/26 13:25:20 henning Exp $

# TARGETS
# pf: feed pfNN.in through pfctl and check wether the output matches pfNN.ok
# selfpf: feed pfctl output through pfctl again and verify it stays the same
# pfail: invalid rulesets pfctl must reject; pfailNN.in and pfailNN.ok
# pfsetup: set up lo1 and perform more tests
# pfr: table tests
# pfsimple: just check wether pfctl accepts a given ruleset, not checking output
# pfload: load ruleset into anchor regress:regress and verify pfctl -vvsr
# pfopt: as target pf, but supply extra command line options

PFTESTS=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27
PFTESTS+=28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50
PFTESTS+=51 52 53 54 55 56 57
PFFAIL=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27
PFSIMPLE=1 2
PFSETUP=1 2 3
PFLOAD=1 2 3 4 5 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 23 24 25 26 27 28 29
PFLOAD+=30 31 32 34 36 38 39 40 44 46 47 49 54 56
# disabled; no altq in anchors
# PFLOAD+=33 35 37 42 43 45 51
# disabled; no table def in anchors
# PFLOAD+=48
# only testing parser, load test would be useless
# PFLOAD+=6 22 41 50 52 53 55 57
PFTABLE=1 2 3 4 5 6 7 8 9 10 11 12 13
PFOPT=1 2 3 4 5

.for n in ${PFFAIL}
PFAIL_TARGETS+=pfail${n}

pfail${n}:
	pfctl -nv -f - < ${.CURDIR}/pfail${n}.in 2>&1 | \
	    diff -u ${.CURDIR}/pfail${n}.ok /dev/stdin

.endfor

pfail:	${PFAIL_TARGETS}
REGRESS_TARGETS+=pfail

.for n in ${PFTESTS}
PF_TARGETS+=pf${n}

pf${n}:
	pfctl -nv -f - < ${.CURDIR}/pf${n}.in | \
	    diff -u ${.CURDIR}/pf${n}.ok /dev/stdin

SELFPF_TARGETS+=selfpf${n}
selfpf${n}:
	pfctl -nv -f - < ${.CURDIR}/pf${n}.ok | \
	    diff -u ${.CURDIR}/pf${n}.ok /dev/stdin

.endfor

pf:	${PF_TARGETS}
selfpf:	${SELFPF_TARGETS}
REGRESS_TARGETS+=pf
REGRESS_TARGETS+=selfpf

.for n in ${PFSIMPLE}
PFSIMPLE_TARGETS+=pfsimple${n}

pfsimple${n}:
	pfctl -nf - < ${.CURDIR}/pfsimple${n}.in

.endfor

pfsimple:	${PFSIMPLE_TARGETS}
REGRESS_TARGETS+=pfsimple

.for n in ${PFLOAD}
PFLOAD_TARGETS+=pfload${n}

pfload${n}:
	${SUDO} pfctl -a regress:regress -f - < ${.CURDIR}/pf${n}.in
	(${SUDO} pfctl -a regress:regress -vvsn; \
	${SUDO} pfctl -a regress:regress -vvsr) | \
	    diff -u ${.CURDIR}/pf${n}.loaded /dev/stdin
	${SUDO} pfctl -a regress:regress -Fn >/dev/null 2>&1
	${SUDO} pfctl -a regress:regress -Fr >/dev/null 2>&1

.endfor

pfload:	${PFLOAD_TARGETS}
REGRESS_TARGETS+=pfload

.for n in ${PFTABLE}
PFR_TARGETS+=pfr${n}

pfr${n}:
	${SUDO} /bin/ksh ${.CURDIR}/pfr.exec ${.CURDIR}/pfr${n}.in | \
	    diff -u ${.CURDIR}/pfr${n}.ok /dev/stdin

.endfor

pfr:	${PFR_TARGETS}
REGRESS_TARGETS+=pfr

.for n in ${PFOPT}
PFOPT_TARGETS+=pfopt${n}

pfopt${n}:
	pfctl -nv -f - `cat ${.CURDIR}/pfopt${n}.opts` \
	    < ${.CURDIR}/pfopt${n}.in | \
	    diff -u ${.CURDIR}/pfopt${n}.ok /dev/stdin

.endfor

pfopt:	${PFOPT_TARGETS}
REGRESS_TARGETS+=pfopt

.for n in ${PFSETUP}
PFSETUP_TARGETS+=pfsetup${n}

pfsetup${n}:
	${SUDO} ${SHELL} ${.CURDIR}/pfsetup${n}.setup
	pfctl -nv -f - < ${.CURDIR}/pfsetup${n}.in | \
	    diff -u ${.CURDIR}/pfsetup${n}.ok /dev/stdin
	${SUDO} ${SHELL} ${.CURDIR}/pfsetup${n}.clean

.endfor

pfsetup:	${PFSETUP_TARGETS}

.ifdef DO_PFSETUP
REGRESS_TARGETS+=pfsetup
.endif

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
