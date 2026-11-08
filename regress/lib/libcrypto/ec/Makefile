#	$OpenBSD: Makefile,v 1.15 2026/11/08 08:53:13 tb Exp $

.ifdef EOPENSSL35
LDADD +=	-Wl,-rpath,/usr/local/lib/eopenssl35 -L/usr/local/lib/eopenssl35
CFLAGS +=	-I/usr/local/include/eopenssl35/
CFLAGS +=	-DOPENSSL_SUPPRESS_DEPRECATED
.endif

PROGS +=	ectest
PROGS +=	ec_arithmetic
PROGS +=	ec_asn1_test
PROGS +=	ec_point_conversion
PROGS +=	ec_builtin_test

LDADD_ec_asn1_test += ${CRYPTO_INT}
LDADD_ec_builtin_test += ${CRYPTO_INT}

LDADD =		-lcrypto
DPADD =		${LIBCRYPTO}

WARNINGS =	Yes
CFLAGS += -DLIBRESSL_CRYPTO_INTERNAL -DLIBRESSL_INTERNAL
CFLAGS += -Wall -Wundef -Werror
CFLAGS += -I${.CURDIR}/../../../../lib/libcrypto/bn
CFLAGS += -I${.CURDIR}/../../../../lib/libcrypto/ec
CFLAGS += -I${.CURDIR}/../test

SRCS_ec_builtin_test = ec_builtin_test.c test.c test_util.c

.PATH: ${.CURDIR}/../test

.include <bsd.regress.mk>
