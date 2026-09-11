#	$OpenBSD$

PROGS +=	mtc_tree_test

LDADD =		${CRYPTO_INT}
DPADD =		${LIBCRYPTO}

CFLAGS +=	-DLIBRESSL_INTERNAL -Wall -Werror
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/bytestring
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/mtc

WARNINGS = Yes

.include <bsd.regress.mk>
