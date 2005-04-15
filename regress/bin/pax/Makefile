# $OpenBSD: Makefile,v 1.2 2005/04/15 07:06:35 markus Exp $

TESTSCRIPTS=t1 t2

.for t in ${TESTSCRIPTS}
REGRESS_TARGETS+=t-${t}
CLEANFILES+=${t}.tar

t-${t}:
	sh ${.CURDIR}/${t}.sh ${.CURDIR} ${.OBJDIR}
.endfor

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
