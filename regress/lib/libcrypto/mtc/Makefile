#	$OpenBSD$

PROGS +=	mtc_tree_test
PROGS +=	mtc_cert_test

run-regress-mtc_cert_test: mtc_cert_test
	./mtc_cert_test ${.CURDIR}

LDADD =		${CRYPTO_INT}
DPADD =		${LIBCRYPTO}

CFLAGS +=	-DLIBRESSL_INTERNAL -Wall -Werror
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/bytestring
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/mtc

WARNINGS = Yes

.include <bsd.regress.mk>
