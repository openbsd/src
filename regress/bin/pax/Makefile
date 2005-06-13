# $OpenBSD: Makefile,v 1.4 2005/06/13 19:22:02 otto Exp $

TESTSCRIPTS=t1 t2 t3 t4

.for t in ${TESTSCRIPTS}
REGRESS_TARGETS+=t-${t}
CLEANFILES+=${t}.tar

t-${t}:
	sh ${.CURDIR}/${t}.sh ${.CURDIR} ${.OBJDIR}
.endfor

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
