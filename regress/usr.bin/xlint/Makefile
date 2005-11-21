# $OpenBSD: Makefile,v 1.1 2005/11/21 18:25:57 cloder Exp $

TEST_MODULES=	1
LINT=		lint
LINTFLAGS?=	-chapbx

.for i in ${TEST_MODULES}
REGRESS_TARGETS+= test-${i}
test-${i}:
	${LINT} ${LINTFLAGS} test-${i}.c 2>&1 | diff -u - ${.CURDIR}/test-${i}.c.exp
.endfor

.PHONY:	${REGRESS_TARGETS}

NOOBJ= Yes

.include <bsd.regress.mk>

