#	$OpenBSD$

PROGS +=	mldsa_tests
PROGS +=	mldsa_unittest

FILE_TEST +=	mldsa44_nist_keygen_tests
FILE_TEST +=	mldsa65_nist_keygen_tests
FILE_TEST +=	mldsa87_nist_keygen_tests
FILE_TEST +=	mldsa44_nist_siggen_tests
FILE_TEST +=	mldsa65_nist_siggen_tests
FILE_TEST +=	mldsa87_nist_siggen_tests

run-regress-mldsa_tests: mldsa_tests
.for f in ${FILE_TEST}
	./mldsa_tests $f ${.CURDIR}/$f.txt
.endfor

.PATH:		${.CURDIR}/../mlkem

SRCS_mldsa_tests =	mldsa_tests.c parse_test_file.c
SRCS_mldsa_unittest =	mldsa_unittest.c mlkem_tests_util.c

LDADD =		${CRYPTO_INT}
DPADD =		${LIBCRYPTO}

CFLAGS +=	-DLIBRESSL_INTERNAL -Wall -Werror
CFLAGS +=	-I${.CURDIR}/../mlkem
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/bytestring
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/mldsa
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/sha

WARNINGS = Yes

.include <bsd.regress.mk>
