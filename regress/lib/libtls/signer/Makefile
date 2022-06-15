#	$OpenBSD: Makefile,v 1.3 2022/06/15 06:14:59 tb Exp $

PROG=	signertest
LDADD=	-lcrypto -lssl ${TLS_INT}
DPADD=	${LIBCRYPTO} ${LIBSSL} ${LIBTLS}

WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Wundef -Werror
CFLAGS+=	-I${.CURDIR}/../../../../lib/libtls/

REGRESS_TARGETS= \
	regress-signertest

regress-signertest: ${PROG}
	./signertest \
	    ${.CURDIR}/../../libssl/certs

.include <bsd.regress.mk>
