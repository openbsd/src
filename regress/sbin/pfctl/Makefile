# $OpenBSD: Makefile,v 1.42 2002/11/25 16:16:07 henning Exp $

PFTESTS=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27
PFTESTS+=28 29 30 31 32 33 34
PFFAIL=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
PFSIMPLE=1 2

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

#REGRESS_TARGETS+=selfpf${n}
selfpf${n}:
	if pfctl -nv -f - < ${.CURDIR}/pf${n}.in | \
	    sed 's/^@[^ ]* //' |\
	    pfctl -nvf - > /dev/null ; then \
		true ; \
	else \
		pfctl -nv -f - < ${.CURDIR}/pf${n}.in; \
		false ; \
	fi

self: selfpf${n}

.endfor

.for n in ${PFSIMPLE}
REGRESS_TARGETS+=pfsimple${n}

pfsimple${n}:
	pfctl -nf - < ${.CURDIR}/pfsimple${n}.in

.endfor

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
