# $OpenBSD: Makefile,v 1.1 2005/04/14 15:17:07 cloder Exp $

TESTSCRIPTS=t1

.for t in ${TESTSCRIPTS}
REGRESS_TARGETS+=t-${t}
CLEANFILES+=${t}.tar

t-${t}:
	sh ${.CURDIR}/${t}.sh ${.CURDIR} ${.OBJDIR}
.endfor

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
