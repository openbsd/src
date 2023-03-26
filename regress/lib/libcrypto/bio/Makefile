#	$OpenBSD: Makefile,v 1.5 2023/03/26 19:14:11 tb Exp $

PROGS +=	bio_asn1
PROGS +=	bio_chain
PROGS +=	bio_host
PROGS +=	bio_mem

LDADD =		-lcrypto
DPADD =		${LIBCRYPTO}
WARNINGS =	Yes
CFLAGS +=	-DLIBRESSL_INTERNAL -Werror
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/bio/

.include <bsd.regress.mk>
