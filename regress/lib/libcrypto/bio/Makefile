#	$OpenBSD: Makefile,v 1.4 2022/12/08 17:49:02 tb Exp $

PROGS +=	bio_chain
PROGS +=	bio_host
PROGS +=	bio_mem

LDADD =		-lcrypto
DPADD =		${LIBCRYPTO}
WARNINGS =	Yes
CFLAGS +=	-DLIBRESSL_INTERNAL -Werror
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/bio/

.include <bsd.regress.mk>
