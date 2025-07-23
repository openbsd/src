#	$OpenBSD: Makefile,v 1.12 2025/07/23 07:43:14 tb Exp $

.ifdef EOPENSSL35
LDADD +=	-Wl,-rpath,/usr/local/lib/eopenssl35 -L/usr/local/lib/eopenssl35
CFLAGS +=	-I/usr/local/include/eopenssl35/
CFLAGS +=	-DOPENSSL_SUPPRESS_DEPRECATED
.endif

PROGS +=	ectest
PROGS +=	ec_asn1_test
PROGS +=	ec_point_conversion

LDADD_ec_asn1_test += ${CRYPTO_INT}

CFLAGS += -I${.CURDIR}/../../../../lib/libcrypto/bn
CFLAGS += -I${.CURDIR}/../../../../lib/libcrypto/ec

LDADD =		-lcrypto
DPADD =		${LIBCRYPTO}
WARNINGS =	Yes
CFLAGS +=	-DLIBRESSL_CRYPTO_INTERNAL -DLIBRESSL_INTERNAL
CFLAGS +=	-Wall -Wundef -Werror

.include <bsd.regress.mk>
