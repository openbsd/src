#	$OpenBSD: Makefile,v 1.14 2022/01/12 08:45:09 tb Exp $

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
CFLAGS+=	-DLIBRESSL_INTERNAL -DLIBRESSL_CRYPTO_INTERNAL
CFLAGS+=	-Wall -Wundef -Werror
CFLAGS+=	-I${.CURDIR}/../../../../lib/libcrypto/asn1
CFLAGS+=	-I${.CURDIR}/../../../../lib/libcrypto/bytestring

LDADD_asn1basic = ${CRYPTO_INT}
LDADD_asn1x509 = ${CRYPTO_INT} # XXX Remove after bump

.for p in ${PROGS}
REGRESS_TARGETS +=	run-$p

run-$p: $p
	@./$p
.endfor

.include <bsd.regress.mk>
