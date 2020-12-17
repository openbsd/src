# $OpenBSD: Makefile,v 1.3 2020/12/17 00:51:11 bluhm Exp $

TESTS =
TESTS +=	conj_test
TESTS +=	fenv_test
TESTS +=	ilogb_test
TESTS +=	lrint_test

PROGS=	${TESTS}
LDADD=	-lm
DPADD=	${LIBM}

.for t in ${TESTS}
REGRESS_TARGETS+=	run-$t
run-$t: $t
	./$t
.endfor

.include <bsd.regress.mk>
