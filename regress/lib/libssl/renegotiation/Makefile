#	$OpenBSD: Makefile,v 1.3 2026/06/14 14:33:36 jsing Exp $

PROG=	renegotiation_test
LDADD=	${SSL_INT} -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror
CFLAGS+=	-I${.CURDIR}/../../../../lib/libssl

REGRESS_TARGETS= \
	regress-renegotiation-test

regress-renegotiation-test: ${PROG}
	./renegotiation_test \
	    ${.CURDIR}/../../libssl/certs/server1-rsa.pem \
	    ${.CURDIR}/../../libssl/certs/server1-rsa-chain.pem \
	    ${.CURDIR}/../../libssl/certs/ca-root-rsa.pem

.include <bsd.regress.mk>
