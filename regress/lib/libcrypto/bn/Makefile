#	$OpenBSD: Makefile,v 1.9 2022/12/01 21:21:51 tb Exp $

PROGS +=	bn_add_sub
PROGS +=	bn_cmp
PROGS +=	bn_isqrt
PROGS +=	bn_mod_exp
PROGS +=	bn_mod_exp2_mont
PROGS +=	bn_mod_sqrt
PROGS +=	bn_mont
PROGS +=	bn_primes
PROGS +=	bn_rand_interval
PROGS +=	bn_to_string
PROGS +=	bn_unit

STATIC_LINK +=	bn_isqrt
STATIC_LINK +=	bn_mod_exp
STATIC_LINK +=	bn_primes
STATIC_LINK +=	bn_to_string
STATIC_LINK +=	bn_rand_interval

LDADD =		-lcrypto
DPADD =		${LIBCRYPTO}
WARNINGS =	Yes
CFLAGS +=	-Wall -Wundef -Werror
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/bn/

.for p in ${PROGS}
REGRESS_TARGETS +=	run-$p
.PHONY +=		run-$p

run-$p: $p
	./$p
.endfor

isqrt-print-tables: bn_isqrt
	@./bn_isqrt -C

# Keep bn_test last since it is special
PROGS +=	bn_test
STATIC_LINK +=	bn_test
CLEANFILES +=	bn_test.out bc.out

REGRESS_TARGETS +=	run-bn_test
.PHONY: 		run-bn_test
run-bn_test bntest.out: bn_test
	./bn_test -out bn_test.out

REGRESS_TARGETS +=	run-bc
.PHONY: 		run-bc
run-bc: bn_test.out
	bc < $> | tee bc.out | grep -v '^0$$'
	! grep -v '^test ' <bc.out | grep -v '^0$$'

.for p in ${STATIC_LINK}
LDADD_$p +=	${CRYPTO_INT}
.endfor

.PHONY: isqrt-print-tables

.include <bsd.regress.mk>
