# $OpenBSD: Makefile,v 1.5 2006/01/25 17:42:08 markus Exp $

TESTSCRIPTS=t1 t2 t3 t4 t5

.for t in ${TESTSCRIPTS}
REGRESS_TARGETS+=t-${t}
CLEANFILES+=${t}.tar

t-${t}:
	sh ${.CURDIR}/${t}.sh ${.CURDIR} ${.OBJDIR}
.endfor

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
