# $OpenBSD: Makefile,v 1.1 2008/08/20 09:29:51 mpf Exp $

TESTSCRIPTS=t1 t2

.for t in ${TESTSCRIPTS}
REGRESS_TARGETS+=t-${t}
CLEANFILES+=${t}.gz

t-${t}:
	sh ${.CURDIR}/${t}.sh ${.CURDIR} ${.OBJDIR}
.endfor

CLEANFILES+=*.test

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
