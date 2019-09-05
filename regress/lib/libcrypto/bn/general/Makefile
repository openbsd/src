#	$OpenBSD: Makefile,v 1.6 2019/09/05 00:59:36 bluhm Exp $

.include "../../Makefile.inc"

PROGS +=	bntest
PROGS +=	bn_to_string

LDADD =		${CRYPTO_INT}
DPADD =		${LIBCRYPTO}
WARNINGS =	Yes
CFLAGS +=	-Werror
CLEANFILES =	bntest.out bc.out

REGRESS_TARGETS += run-bntest
run-bntest bntest.out: bntest
	@echo '\n======== $@ ========'
	./bntest -out bntest.out

REGRESS_TARGETS += run-bc
run-bc: bntest.out
	@echo '\n======== $@ ========'
	bc < bntest.out | tee bc.out | grep -v '^0$$'
	! grep -v '^test ' <bc.out | grep -v '^0$$'

REGRESS_TARGETS += run-bn_to_string
run-bn_to_string: bn_to_string
	@echo '\n======== $@ ========'
	./bn_to_string

.include <bsd.regress.mk>
