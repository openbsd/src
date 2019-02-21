# $OpenBSD: Makefile,v 1.1.1.1 2019/02/21 16:14:03 bluhm Exp $

TESTS =
TESTS +=	conj_test
TESTS +=	fenv_test
TESTS +=	lrint_test

PROGS=	${TESTS}
LDADD=	-lm
DPADD=	${LIBM}

.for t in ${TESTS}
REGRESS_TARGETS+=	run-$t
run-$t: $t
	@echo "\n======== $@ ========"
	./$t
.endfor

.include <bsd.regress.mk>
