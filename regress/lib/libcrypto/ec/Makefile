#	$OpenBSD: Makefile,v 1.7 2023/04/17 20:36:08 tb Exp $

PROGS +=		ectest
PROGS +=		ec_asn1_test
PROGS +=		ec_point_conversion

LDADD =			${CRYPTO_INT}
DPADD =			${LIBCRYPTO}
WARNINGS =		Yes
CFLAGS +=		-DLIBRESSL_CRYPTO_INTERNAL -DLIBRESSL_INTERNAL
CFLAGS +=		-Wall -Wundef -Werror

.include <bsd.regress.mk>
