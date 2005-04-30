# $OpenBSD: Makefile,v 1.3 2005/04/30 17:36:58 otto Exp $

TESTSCRIPTS=t1 t2 t3

.for t in ${TESTSCRIPTS}
REGRESS_TARGETS+=t-${t}
CLEANFILES+=${t}.tar

t-${t}:
	sh ${.CURDIR}/${t}.sh ${.CURDIR} ${.OBJDIR}
.endfor

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
