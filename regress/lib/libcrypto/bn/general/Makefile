#	$OpenBSD: Makefile,v 1.14 2022/07/25 20:48:57 tb Exp $

.include "../../Makefile.inc"

PROGS +=	bntest
PROGS +=	bn_isqrt
PROGS +=	bn_mod_exp2_mont
PROGS +=	bn_mod_sqrt
PROGS +=	bn_primes
PROGS +=	bn_to_string

DPADD +=	${LIBCRYPTO}
LDFLAGS +=	-lcrypto
WARNINGS =	Yes
CFLAGS +=	-Werror
CFLAGS +=	-I${.CURDIR}/../../../../../lib/libcrypto/bn/
CLEANFILES =	bntest.out bc.out

LDADD_bntest = ${CRYPTO_INT}
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

LDADD_bn_to_string = ${CRYPTO_INT}
REGRESS_TARGETS += run-bn_to_string
run-bn_to_string: bn_to_string
	./bn_to_string

LDADD_bn_isqrt = ${CRYPTO_INT}
REGRESS_TARGETS += run-bn_isqrt
run-bn_isqrt: bn_isqrt
	./bn_isqrt

print-tables: bn_isqrt
	@./bn_isqrt -C

.include <bsd.regress.mk>
