#	$OpenBSD: Makefile,v 1.16 2022/12/09 09:56:41 tb Exp $

PROGS +=	bn_add_sub
PROGS +=	bn_cmp
PROGS +=	bn_isqrt
PROGS +=	bn_mod_exp
PROGS +=	bn_mod_exp2_mont
PROGS +=	bn_mod_exp_zero
PROGS +=	bn_mod_sqrt
PROGS +=	bn_mont
PROGS +=	bn_primes
PROGS +=	bn_rand_interval
PROGS +=	bn_test
PROGS +=	bn_to_string
PROGS +=	bn_unit

STATIC_LINK +=	bn_isqrt
STATIC_LINK +=	bn_mod_exp
STATIC_LINK +=	bn_mod_exp_zero
STATIC_LINK +=	bn_primes
STATIC_LINK +=	bn_rand_interval
STATIC_LINK +=	bn_test
STATIC_LINK +=	bn_to_string

LDADD =		-lcrypto
DPADD =		${LIBCRYPTO}
WARNINGS =	Yes
CFLAGS +=	-Wall -Wundef -Werror
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/bn/

# Use default targets from bsd.regress.mk unless overridden below
REGRESS_TARGETS = ${PROGS:S/^/run-regress-/}

# Verify that the bn_isqrt -C output isn't changed by accident.
isqrt-print-tables: bn_isqrt
	@./bn_isqrt -C
.PHONY: isqrt-print-tables

bn_isqrt_gen_tables.txt: bn_isqrt
	${.MAKE} -C ${.CURDIR} isqrt-print-tables > $@.tmp
	mv -f $@.tmp $@

REGRESS_TARGETS +=	run-bn_isqrt_check_tables
.PHONY:			run-bn_isqrt_check_tables
run-bn_isqrt_check_tables: bn_isqrt_gen_tables.txt
	diff -pu ${.CURDIR}/bn_isqrt_tables.txt bn_isqrt_gen_tables.txt

CLEANFILES += bn_isqrt_gen_tables.txt.tmp bn_isqrt_gen_tables.txt

# bn_test is special, so override bsd.regress.mk's run-regress-bn_test
run-regress-bn_test bntest.out: bn_test
	./bn_test -out bn_test.out

REGRESS_TARGETS +=	run-bc
.PHONY: 		run-bc
run-bc: bn_test.out
	bc < $> | tee bc.out | grep -v '^0$$'
	! grep -v '^test ' <bc.out | grep -v '^0$$'

CLEANFILES +=	bn_test.out bc.out

.for p in ${STATIC_LINK}
LDADD_$p +=	${CRYPTO_INT}
.endfor

.include <bsd.regress.mk>
