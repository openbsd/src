#	$OpenBSD: Makefile,v 1.13 2022/01/12 07:55:25 tb Exp $

PROGS = \
	asn1api \
	asn1basic \
	asn1complex \
	asn1evp \
	asn1string_copy \
	asn1time \
	asn1x509 \
	rfc5280time \
	string_table

DPADD+=		${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
LDFLAGS+=	-lcrypto
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Wundef -Werror
CFLAGS+=	-I${.CURDIR}/../../../../lib/libcrypto/asn1
CFLAGS+=	-I${.CURDIR}/../../../../lib/libcrypto/bytestring

LDADD_asn1basic = ${CRYPTO_INT}

.for p in ${PROGS}
REGRESS_TARGETS +=	run-$p

run-$p: $p
	@./$p
.endfor

.include <bsd.regress.mk>
