#	$OpenBSD$

PROGS +=	mtc_tree_test
PROGS +=	mtc_cert_test
PROGS +=	mtc_ca_test
PROGS +=	mtc_api_test

run-regress-mtc_cert_test: mtc_cert_test
	./mtc_cert_test ${.CURDIR}

run-regress-mtc_api_test: mtc_api_test
	./mtc_api_test ${.CURDIR}

LDADD =		${CRYPTO_INT}
DPADD =		${LIBCRYPTO}

CFLAGS +=	-DLIBRESSL_INTERNAL -Wall -Werror
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/bytestring
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/mtc

WARNINGS = Yes

.include <bsd.regress.mk>
