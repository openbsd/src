# $OpenBSD: Makefile,v 1.11 2005/12/06 14:27:57 markus Exp $

# TARGETS
# ipsec: feed ipsecNN.in through ipsecctl and check wether the output matches
# 	 ipsecNN.ok
# tcpmd5: same as above, but for tcpmd5 rules
# sa:	 same as above, but for SA rules.
# ike:	 same as above, but for ike rules.

IPSECTESTS=1 2 3 4 5 6 7 8 9 10 11 12 13 14
TCPMD5TESTS=1 2 3
SATESTS=1 2 3 4 5 6 7 8 9 10
IKETESTS=1 2 3 4 5 6 7 8

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

.for n in ${IKETESTS}
IKE_TARGETS+=ike${n}

ike${n}:
	cat ${.CURDIR}/ike${n}.in | sed -e 's,DIR,${.CURDIR},g' | \
	    ipsecctl -nv -f - | diff -u ${.CURDIR}/ike${n}.ok /dev/stdin
.endfor

ipsec:		${IPSEC_TARGETS}
REGRESS_TARGETS+=ipsec

tcpmd5:		${TCPMD5_TARGETS}
REGRESS_TARGETS+=tcpmd5

sa:		${SA_TARGETS}
REGRESS_TARGETS+=sa

ike:		${IKE_TARGETS}
REGRESS_TARGETS+=ike

alltests: ${REGRESS_TARGETS}

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
