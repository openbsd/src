#	$OpenBSD: Makefile,v 1.19 2022/06/28 08:41:17 beck Exp $

PROGS = \
	asn1api \
	asn1basic \
	asn1complex \
	asn1evp \
	asn1object \
	asn1string_copy \
	asn1_string_to_utf8 \
	asn1time \
	asn1x509 \
	rfc5280time \
	string_table

DPADD+=		${LIBCRYPTO}
WARNINGS=	Yes
LDFLAGS+=	-lcrypto
LDADD= ${CRYPTO_INT}
CFLAGS+=	-DLIBRESSL_INTERNAL
CFLAGS+=	-Wall -Wundef -Werror
CFLAGS+=	-I${.CURDIR}/../../../../lib/libcrypto/asn1
CFLAGS+=	-I${.CURDIR}/../../../../lib/libcrypto/bytestring

LDADD_asn1basic = ${CRYPTO_INT}

.for p in ${PROGS}
REGRESS_TARGETS +=	run-$p

run-$p: $p
	@./$p
.endfor

.include <bsd.regress.mk>
