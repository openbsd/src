# $OpenBSD: Makefile,v 1.185 2005/01/27 15:30:35 dhartmei Exp $

# TARGETS
# pf: feed pfNN.in through pfctl and check wether the output matches pfNN.ok
# selfpf: feed pfctl output through pfctl again and verify it stays the same
# pfail: invalid rulesets pfctl must reject; pfailNN.in and pfailNN.ok
# pfsetup: set up lo1 and perform more tests
# pfr: table tests
# pfsimple: just check wether pfctl accepts a given ruleset, not checking output
# pfload: load ruleset into anchor regress and verify pfctl -vvsr
# pfoptimize: as pfload, with -o flag to pfctl
# pfopt: as target pf, but supply extra command line options

PFTESTS=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27
PFTESTS+=28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50
PFTESTS+=51 52 53 54 55 56 57 58 59 60 61 62 63 64 65 66 67 68 69 70 71 72 73
PFTESTS+=74 75 76 77 78 79 80 81 82 83 84 85 86 87 88 89
PFFAIL=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 23 24 25 27
PFFAIL+=28 29 30 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49
PFSIMPLE=1 2
PFSETUP=1 2 3 4
PFLOAD=1 2 3 4 5 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 23 24 25 26 27 28 29
PFLOAD+=30 31 32 34 36 38 39 40 44 46 47 48 49 54 56 60 61 65 66 67 68 69 70 71
PFLOAD+=72 73 74 75 76 77 78 79 80 81 82 84 87 88 89
PFALTQ=1 2 3 4 5 6 7 8 9 10 11 12 13 14
# disabled; no altq in anchors
# PFLOAD+=33 35 37 42 43 45 51 58 59 62 63 64
# only testing parser, load test would be useless
# PFLOAD+=6 22 41 50 52 53 55 57 83 85 86
PFTABLE=1 2 3 4 5 6 7 8 9 10 11 12 13
PFOPT=1 2 3 4 5
PFIF2IP=1 2 3

.MAIN: all

.ifmake !obj && !clean && !cleandir && !depend && !regress
.if (${.TARGET} != all && ! make(all)) || (${.TARGET} == all)
.BEGIN:
	-${SUDO} ifconfig lo1000000 create
	-${SUDO} ifconfig tun1000000 create
	-${SUDO} ifconfig tun1000001 create

.END:
	-${SUDO} ifconfig lo1000000 destroy
	-${SUDO} ifconfig tun1000000 destroy
	-${SUDO} ifconfig tun1000001 destroy

.INTERRUPT:
	-${SUDO} ifconfig lo1000000 destroy
	-${SUDO} ifconfig tun1000000 destroy
	-${SUDO} ifconfig tun1000001 destroy
.endif
.endif

.for n in ${PFFAIL}
PFAIL_TARGETS+=pfail${n}
PFAIL_UPDATES+=pfail${n}-update

pfail${n}:
	pfctl -nv -f - < ${.CURDIR}/pfail${n}.in 2>&1 | \
	    diff -u ${.CURDIR}/pfail${n}.ok /dev/stdin

pfail${n}-update:
	if pfctl -nv -f - < ${.CURDIR}/pfail${n}.in > \
	    ${.CURDIR}/pfail${n}.ok 2>&1; then \
		true; \
	fi;
	
.endfor

pfail:		${PFAIL_TARGETS}
pfail-update:	${PFAIL_UPDATES}
REGRESS_TARGETS+=pfail
UPDATE_TARGETS+=pfail-update

.for n in ${PFTESTS}
PF_TARGETS+=pf${n}
PF_UPDATES+=pf${n}-update

pf${n}:
	pfctl -nv -f - < ${.CURDIR}/pf${n}.in | \
	    diff -u ${.CURDIR}/pf${n}.ok /dev/stdin

pf${n}-update:
	pfctl -nv -f - < ${.CURDIR}/pf${n}.in > ${.CURDIR}/pf${n}.ok

SELFPF_TARGETS+=selfpf${n}
selfpf${n}:
	pfctl -nv -f - < ${.CURDIR}/pf${n}.ok | \
	    diff -u ${.CURDIR}/pf${n}.ok /dev/stdin

.endfor

pf:		${PF_TARGETS}
selfpf:		${SELFPF_TARGETS}
pf-update:	${PF_UPDATES}
REGRESS_TARGETS+=pf
REGRESS_TARGETS+=selfpf
UPDATE_TARGETS+=pf-update

.for n in ${PFSIMPLE}
PFSIMPLE_TARGETS+=pfsimple${n}

pfsimple${n}:
	pfctl -nf - < ${.CURDIR}/pfsimple${n}.in

.endfor

pfsimple:	${PFSIMPLE_TARGETS}
REGRESS_TARGETS+=pfsimple

.for n in ${PFLOAD}
PFLOAD_TARGETS+=pfload${n}
PFLOAD_UPDATES+=pfload${n}-update

pfload${n}:
	${SUDO} pfctl -a regress -f - < ${.CURDIR}/pf${n}.in
	(${SUDO} pfctl -a regress -gvvsn; \
	${SUDO} pfctl -a regress -gvvsr) | \
	    diff -u ${.CURDIR}/pf${n}.loaded /dev/stdin
	${SUDO} pfctl -a regress -Fn >/dev/null 2>&1
	${SUDO} pfctl -a regress -Fr >/dev/null 2>&1

pfload${n}-update:
	${SUDO} pfctl -a regress -f - < ${.CURDIR}/pf${n}.in
	(${SUDO} pfctl -a regress -gvvsn; \
	${SUDO} pfctl -a regress -gvvsr) > ${.CURDIR}/pf${n}.loaded
	${SUDO} pfctl -a regress -Fn >/dev/null 2>&1
	${SUDO} pfctl -a regress -Fr >/dev/null 2>&1

.endfor

pfload:		${PFLOAD_TARGETS}
pfload-update:	${PFLOAD_UPDATES}
REGRESS_TARGETS+=pfload
REGRESS_ROOT_TARGETS+=pfload
UPDATE_TARGETS+=pfload-update

.for n in ${PFLOAD}
PFOPTIMIZE_TARGETS+=pfoptimize${n}
PFOPTIMIZE_UPDATES+=pfoptimize${n}-update

pfoptimize${n}:
	${SUDO} pfctl -o -a regress -f - < ${.CURDIR}/pf${n}.in
	(${SUDO} pfctl -a regress -gvvsn; \
	${SUDO} pfctl -a regress -gvvsr) | \
	    diff -u ${.CURDIR}/pf${n}.optimized /dev/stdin
	${SUDO} pfctl -a regress -Fn >/dev/null 2>&1
	${SUDO} pfctl -a regress -Fr >/dev/null 2>&1

pfoptimize${n}-update:
	${SUDO} pfctl -o -a regress -f - < ${.CURDIR}/pf${n}.in
	(${SUDO} pfctl -a regress -gvvsn; \
	${SUDO} pfctl -a regress -gvvsr) > ${.CURDIR}/pf${n}.optimized
	${SUDO} pfctl -a regress -Fn >/dev/null 2>&1
	${SUDO} pfctl -a regress -Fr >/dev/null 2>&1

.endfor

pfoptimize:		${PFOPTIMIZE_TARGETS}
pfoptimize-update:	${PFOPTIMIZE_UPDATES}
REGRESS_TARGETS+=pfoptimize
REGRESS_ROOT_TARGETS+=pfoptimize
UPDATE_TARGETS+=pfoptimize-update

.for n in ${PFTABLE}
PFR_TARGETS+=pfr${n}
PFR_UPDATES+=pfr${n}-update

pfr${n}:
	${SUDO} /bin/ksh ${.CURDIR}/pfr.exec ${.CURDIR}/pfr${n}.in | \
	    diff -u ${.CURDIR}/pfr${n}.ok /dev/stdin

pfr${n}-update:
	${SUDO} /bin/ksh ${.CURDIR}/pfr.exec ${.CURDIR}/pfr${n}.in > \
	    ${.CURDIR}/pfr${n}.ok

.endfor

pfr:		${PFR_TARGETS}
pfr-update:	${PFR_UPDATES}
NODEFAULT_TARGETS+=pfr
REGRESS_ROOT_TARGETS+=pfr

.for n in ${PFIF2IP}
PFI_TARGETS+=pfi${n}
PFI_UPDATES+=pfi${n}-update

pfi${n}:
	xargs ${SUDO} /bin/ksh ${.CURDIR}/if2ip <${.CURDIR}/pfi${n}.in | \
            diff -u ${.CURDIR}/pfi${n}.ok /dev/stdin

pfi${n}-update:
	xargs ${SUDO} /bin/ksh ${.CURDIR}/if2ip <${.CURDIR}/pfi${n}.in \
	    > ${.CURDIR}/pfi${n}.ok

.endfor

pfi:            ${PFI_TARGETS}
pfi-update:     ${PFI_UPDATES}
# REGRESS_TARGETS+=pfi
# REGRESS_ROOT_TARGETS+=pfi
# UPDATE_TARGETS+=pfi-update

.for n in ${PFOPT}
PFOPT_TARGETS+=pfopt${n}
PFOPT_UPDATES+=pfopt${n}-update

pfopt${n}:
	pfctl -nv -f - `cat ${.CURDIR}/pfopt${n}.opts` \
	    < ${.CURDIR}/pfopt${n}.in | \
	    diff -u ${.CURDIR}/pfopt${n}.ok /dev/stdin

pfopt${n}-update:
	pfctl -nv -f - `cat ${.CURDIR}/pfopt${n}.opts` \
	    < ${.CURDIR}/pfopt${n}.in > ${.CURDIR}/pfopt${n}.ok

.endfor

pfopt:		${PFOPT_TARGETS}
pfopt-update:	${PFOPT_UPDATES}
REGRESS_TARGETS+=pfopt
UPDATE_TARGETS+=pfopt-update

.for n in ${PFSETUP}
PFSETUP_TARGETS+=pfsetup${n}
PFSETUP_UPDATES+=pfsetup${n}-update

pfsetup${n}:
	${SUDO} ${SHELL} ${.CURDIR}/pfsetup${n}.setup
	pfctl -nv -f - < ${.CURDIR}/pfsetup${n}.in | \
	    diff -u ${.CURDIR}/pfsetup${n}.ok /dev/stdin
	${SUDO} ${SHELL} ${.CURDIR}/pfsetup${n}.clean

pfsetup${n}-update:
	${SUDO} ${SHELL} ${.CURDIR}/pfsetup${n}.setup
	pfctl -nv -f - < ${.CURDIR}/pfsetup${n}.in \
	    > ${.CURDIR}/pfsetup${n}.ok
	${SUDO} ${SHELL} ${.CURDIR}/pfsetup${n}.clean

.endfor

pfsetup:	${PFSETUP_TARGETS}
pfsetup-update:	${PFSETUP_UPDATES}

NODEFAULT_TARGETS+=pfsetup
REGRESS_ROOT_TARGETS+=pfsetup

.for n in ${PFALTQ}
PFALTQ_TARGETS+=pfaltq${n}
PFALTQ_UPDATES+=pfaltq${n}-update

pfaltq${n}:
	${SUDO} pfctl -Fa >/dev/null 2>&1
	${SUDO} pfctl -f - < ${.CURDIR}/pfaltq${n}.in
	( ${SUDO} pfctl -gsq; \
	${SUDO} pfctl -gsr ) | \
	    diff -u ${.CURDIR}/pfaltq${n}.ok /dev/stdin
	${SUDO} pfctl -Fa >/dev/null 2>&1

pfaltq${n}-update:
	${SUDO} pfctl -Fa >/dev/null 2>&1
	${SUDO} pfctl -f - < ${.CURDIR}/pfaltq${n}.in
	( ${SUDO} pfctl -gsq; \
	${SUDO} pfctl -gsr ) > ${.CURDIR}/pfaltq${n}.ok
	${SUDO} pfctl -Fa >/dev/null 2>&1

.endfor

pfaltq:		${PFALTQ_TARGETS}
pfaltq-update:	${PFALTQ_UPDATES}

NODEFAULT_TARGETS+=pfaltq
REGRESS_ROOT_TARGETS+=pfaltq

update:	${UPDATE_TARGETS}

alltests: ${REGRESS_TARGETS} ${NODEFAULT_TARGETS}

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
