#       $OpenBSD: Makefile,v 1.7 2024/12/20 01:53:46 tb Exp $

PROGS +=	mlkem768_decap_tests
PROGS +=	mlkem768_encap_tests
PROGS +=	mlkem768_iteration_test
PROGS +=	mlkem768_keygen_tests
PROGS +=	mlkem768_nist_decap_tests
PROGS +=	mlkem768_nist_keygen_tests
PROGS +=	mlkem1024_decap_tests
PROGS +=	mlkem1024_encap_tests
PROGS +=	mlkem1024_iteration_test
PROGS +=	mlkem1024_keygen_tests
PROGS +=	mlkem1024_nist_decap_tests
PROGS +=	mlkem1024_nist_keygen_tests
PROGS +=	mlkem_unittest

# Link test programs with mlkem_tests_util.c and use custom target
.for p in ${PROGS}
SRCS_$p +=	$p.c mlkem_tests_util.c

REGRESS_TARGETS += run-$p
run-$p: $p
	./$p ${.CURDIR}/$p.txt
.endfor

LDADD =		${CRYPTO_INT}
DPADD =		${LIBCRYPTO}

CFLAGS +=	-DLIBRESSL_INTERNAL -Wall -Werror
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/bytestring
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/mlkem
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/sha
CFLAGS +=	-DLIBRESSL_INTERNAL

WARNINGS = Yes

.include <bsd.regress.mk>
