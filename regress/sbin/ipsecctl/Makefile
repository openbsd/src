# $OpenBSD: Makefile,v 1.4 2005/08/09 12:45:56 hshoexer Exp $

# TARGETS
# ipsec: feed ipsecNN.in through ipsecctl and check wether the output matches
# 	 ipsecNN.ok
# tcpmd5: same as above, but for tcpmd5 rules
# sa:	 same as above, but for SA rules.

IPSECTESTS=1 2 3 4 5 6 7 8 9 10 11 12
TCPMD5TESTS=1 2
SATESTS=1 2 3 4 5 6

SHELL=/bin/sh

.MAIN: all

.for n in ${IPSECTESTS}
IPSEC_TARGETS+=ipsec${n}

ipsec${n}:
	ipsecctl -nv -f - < ${.CURDIR}/ipsec${n}.in | \
	    diff -u ${.CURDIR}/ipsec${n}.ok /dev/stdin

.endfor

.for n in ${TCPMD5TESTS}
TCPMD5_TARGETS+=tcpmd5${n}

tcpmd5${n}:
	ipsecctl -nv -f - < ${.CURDIR}/tcpmd5${n}.in | \
	    diff -u ${.CURDIR}/tcpmd5${n}.ok /dev/stdin
.endfor

.for n in ${SATESTS}
SA_TARGETS+=sa${n}

sa${n}:
	ipsecctl -nv -f - < ${.CURDIR}/sa${n}.in | \
	    diff -u ${.CURDIR}/sa${n}.ok /dev/stdin
.endfor

ipsec:		${IPSEC_TARGETS}
REGRESS_TARGETS+=ipsec

tcpmd5:		${TCPMD5_TARGETS}
REGRESS_TARGETS+=tcpmd5

sa:		${SA_TARGETS}
REGRESS_TARGETS+=sa

alltests: ${REGRESS_TARGETS}

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
