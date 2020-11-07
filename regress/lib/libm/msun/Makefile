# $OpenBSD: Makefile,v 1.2 2020/11/07 08:58:28 kettenis Exp $

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
	@echo "\n======== $@ ========"
	./$t
.endfor

.include <bsd.regress.mk>
