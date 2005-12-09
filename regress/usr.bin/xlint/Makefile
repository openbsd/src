# $OpenBSD: Makefile,v 1.13 2005/12/09 04:30:58 cloder Exp $

TEST_MODULES=	1 2 3 4 5 6 7 8 9 10 11 12
LINT=		lint
LINTFLAGS?=	-chapbx

.for i in ${TEST_MODULES}
REGRESS_TARGETS+= test-${i}
test-${i}:
	${LINT} ${LINTFLAGS} test-${i}.c 2>&1 | diff -u ${.CURDIR}/test-${i}.c.exp -
.endfor

.PHONY:	${REGRESS_TARGETS}

NOOBJ= Yes

.include <bsd.regress.mk>

