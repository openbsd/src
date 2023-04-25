#	$OpenBSD: Makefile,v 1.7 2023/04/25 19:48:24 tb Exp $

PROGS +=	bio_asn1
PROGS +=	bio_chain
PROGS +=	bio_host
PROGS +=	bio_mem

LDADD =		-lcrypto
DPADD =		${LIBCRYPTO}
WARNINGS =	Yes
CFLAGS +=	-DLIBRESSL_INTERNAL -Werror
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/asn1/
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/bio/
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/bytestring/

LDADD_bio_asn1 = ${CRYPTO_INT}

.include <bsd.regress.mk>
