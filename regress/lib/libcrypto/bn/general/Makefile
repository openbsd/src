#	$OpenBSD: Makefile,v 1.12 2022/06/19 17:08:19 tb Exp $

.include "../../Makefile.inc"

PROGS +=	bntest
PROGS +=	bn_mod_exp2_mont
PROGS +=	bn_mod_sqrt
PROGS +=	bn_primes
PROGS +=	bn_to_string

LDADD =		${LIBCRYPTO}
DPADD =		${LIBCRYPTO}
WARNINGS =	Yes
CFLAGS +=	-Werror
CFLAGS +=	-I${.CURDIR}/../../../../../lib/libcrypto/bn/
CLEANFILES =	bntest.out bc.out

REGRESS_TARGETS += run-bntest
run-bntest bntest.out: bntest
	./bntest -out bntest.out

REGRESS_TARGETS += run-bc
run-bc: bntest.out
	bc < bntest.out | tee bc.out | grep -v '^0$$'
	! grep -v '^test ' <bc.out | grep -v '^0$$'

REGRESS_TARGETS += run-bn_mod_exp2_mont
run-bn_mod_exp2_mont: bn_mod_exp2_mont
	./bn_mod_exp2_mont

REGRESS_TARGETS += run-bn_mod_sqrt
run-bn_mod_sqrt: bn_mod_sqrt
	./bn_mod_sqrt

REGRESS_TARGETS += run-bn_primes
run-bn_primes: bn_primes
	./bn_primes

REGRESS_TARGETS += run-bn_to_string
run-bn_to_string: bn_to_string
	./bn_to_string

.include <bsd.regress.mk>
