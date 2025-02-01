#	$OpenBSD: Makefile,v 1.1 2025/02/01 12:21:52 jsing Exp $

PROG=	renegotiation_test
LDADD=	-lssl -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

REGRESS_TARGETS= \
	regress-renegotiation-test

regress-renegotiation-test: ${PROG}
	./renegotiation_test \
	    ${.CURDIR}/../../libssl/certs/server.pem \
	    ${.CURDIR}/../../libssl/certs/server.pem \
	    ${.CURDIR}/../../libssl/certs/ca.pem

.include <bsd.regress.mk>
