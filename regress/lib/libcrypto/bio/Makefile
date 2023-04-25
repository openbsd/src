#	$OpenBSD: Makefile,v 1.6 2023/04/25 15:29:17 tb Exp $

PROGS +=	bio_asn1
PROGS +=	bio_chain
PROGS +=	bio_host
PROGS +=	bio_mem

LDADD =		-lcrypto
DPADD =		${LIBCRYPTO}
WARNINGS =	Yes
CFLAGS +=	-DLIBRESSL_INTERNAL -Werror
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/bio/

LDADD_bio_asn1 = ${CRYPTO_INT}

.include <bsd.regress.mk>
