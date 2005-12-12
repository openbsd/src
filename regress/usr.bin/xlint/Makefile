# $OpenBSD: Makefile,v 1.17 2005/12/12 23:37:51 cloder Exp $

TEST_MODULES=	1 2 3 4 5 6 7 8 9 10 12 13 14
LINT=		lint
LINTFLAGS?=	-chapbx

.for i in ${TEST_MODULES}
REGRESS_TARGETS+= test-${i}
GENERATE_TARGETS+=generate-${i}
test-${i}:
	${LINT} ${LINTFLAGS} test-${i}.c 2>&1 | diff -u ${.CURDIR}/test-${i}.c.exp -

generate-${i}:
	${LINT} ${LINTFLAGS} test-${i}.c 2>&1 > ${.CURDIR}/test-${i}.c.exp
.endfor

.PHONY:	${REGRESS_TARGETS} generate ${GENERATE_TARGETS}

generate: ${GENERATE_TARGETS}


NOOBJ= Yes

.include <bsd.regress.mk>

