# $OpenBSD: Makefile,v 1.21 2002/07/06 17:08:41 henning Exp $

.include <bsd.obj.mk>

PFTESTS=1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22
PFFAIL=1 2 3 4 5 6 7 8 9 10 11

PFDIR=${.CURDIR}/../../../sbin/pfctl/${__objdir}/

.for n in ${PFFAIL}
REGRESSTARGETS+=pfail${n}

.BEGIN:
.if !exists(${PFDIR}pfctl)
.undef PFDIR

.endif

pfail${n}:
	if ${PFDIR}pfctl -nv -f - < ${.CURDIR}/pfail${n}.in > /dev/null 2>&1 ; then \
		false ; \
	fi

.endfor

.for n in ${PFTESTS}
REGRESSTARGETS+=pf${n}

pf${n}:
	${PFDIR}pfctl -nv -f - < ${.CURDIR}/pf${n}.in | \
	    cmp -s ${.CURDIR}/pf${n}.ok /dev/stdin

.endfor

.PHONY: ${REGRESSTARGETS}

.include <bsd.regress.mk>
