# $OpenBSD: Makefile,v 1.33 2006/06/15 11:05:58 hshoexer Exp $

# TARGETS
# ipsec: feed ipsecNN.in through ipsecctl and check wether the output matches
# 	 ipsecNN.ok
# tcpmd5: same as above, but for tcpmd5 rules
# sa:	 same as above, but for SA rules.
# ike:	 same as above, but for ike rules.

IPSECTESTS=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
IPSECTESTS+=25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44
IPSECTESTS+=51 52
TCPMD5TESTS=1 2 3
SATESTS=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
SAFAIL=1 2
IPSECFAIL=1 2
IKEFAIL=1 3 4 5
IKETESTS=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
IKETESTS+=16 17 18 19 20 21 22 23
IKETESTS+=29 30 31 32 33 34 35 36 37 38 39 40
IKETESTS+=41 42 43 46

SHELL=/bin/sh

.MAIN: all

.for n in ${IPSECTESTS}
IPSEC_TARGETS+=ipsec${n}

ipsec${n}:
	cat ${.CURDIR}/ipsec${n}.in | sed -e 's,DIR,${.CURDIR},g' | \
	    ipsecctl -nv -f - | diff -u ${.CURDIR}/ipsec${n}.ok /dev/stdin
.endfor

.for n in ${TCPMD5TESTS}
TCPMD5_TARGETS+=tcpmd5${n}

tcpmd5${n}:
	cat ${.CURDIR}/tcpmd5${n}.in | sed -e 's,DIR,${.CURDIR},g' | \
	    ipsecctl -nv -f - | diff -u ${.CURDIR}/tcpmd5${n}.ok /dev/stdin
.endfor

.for n in ${SATESTS}
SA_TARGETS+=sa${n}

sa${n}:
	cat ${.CURDIR}/sa${n}.in | sed -e 's,DIR,${.CURDIR},g' | \
	    ipsecctl -nv -f - | diff -u ${.CURDIR}/sa${n}.ok /dev/stdin
.endfor

.for n in ${SAFAIL}
SAFAIL_TARGETS+=safail${n}

safail${n}:
	cat ${.CURDIR}/safail${n}.in | sed -e 's,DIR,${.CURDIR},g' | \
	    ipsecctl -nv -f - 2>&1 | diff -u ${.CURDIR}/safail${n}.ok /dev/stdin
.endfor

.for n in ${IPSECFAIL}
IPSECFAIL_TARGETS+=ipsecfail${n}

ipsecfail${n}:
	cat ${.CURDIR}/ipsecfail${n}.in | sed -e 's,DIR,${.CURDIR},g' | \
	    ipsecctl -nv -f - 2>&1 | diff -u ${.CURDIR}/ipsecfail${n}.ok /dev/stdin
.endfor

.for n in ${IKETESTS}
IKE_TARGETS+=ike${n}

ike${n}:
	cat ${.CURDIR}/ike${n}.in | sed -e 's,DIR,${.CURDIR},g' | \
	    ipsecctl -nv -f - | diff -u ${.CURDIR}/ike${n}.ok /dev/stdin
.endfor

.for n in ${IKEFAIL}
IKEFAIL_TARGETS+=ikefail${n}

ikefail${n}:
	cat ${.CURDIR}/ikefail${n}.in | sed -e 's,DIR,${.CURDIR},g' | \
	    ipsecctl -nv -f - 2>&1 | diff -u ${.CURDIR}/ikefail${n}.ok /dev/stdin
.endfor

ipsec:		${IPSEC_TARGETS}
REGRESS_TARGETS+=ipsec

tcpmd5:		${TCPMD5_TARGETS}
REGRESS_TARGETS+=tcpmd5

sa:		${SA_TARGETS}
REGRESS_TARGETS+=sa

safail:		${SAFAIL_TARGETS}
REGRESS_TARGETS+=safail

ipsecfail:	${IPSECFAIL_TARGETS}
REGRESS_TARGETS+=ipsecfail

ike:		${IKE_TARGETS}
REGRESS_TARGETS+=ike

ikefail:	${IKEFAIL_TARGETS}
REGRESS_TARGETS+=ikefail

alltests: ${REGRESS_TARGETS}

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
