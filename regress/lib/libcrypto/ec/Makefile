#	$OpenBSD: Makefile,v 1.11 2025/03/08 20:09:35 tb Exp $

.ifdef EOPENSSL33
LDADD +=	-Wl,-rpath,/usr/local/lib/eopenssl33 -L/usr/local/lib/eopenssl33
CFLAGS +=	-I/usr/local/include/eopenssl33/
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
