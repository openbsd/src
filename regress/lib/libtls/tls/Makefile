# $OpenBSD: Makefile,v 1.4 2026/09/20 17:26:14 beck Exp $

PROG=	tlstest
LDADD=	-lcrypto -lssl -ltls
DPADD=	${LIBCRYPTO} ${LIBSSL} ${LIBTLS}

WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

REGRESS_TARGETS= \
	regress-tlstest

regress-tlstest: ${PROG}
	./tlstest \
	    ${.CURDIR}/../../libssl/certs/ca-root-rsa.pem \
	    ${.CURDIR}/../../libssl/certs/server1-rsa-chain.pem \
	    ${.CURDIR}/../../libssl/certs/server1-rsa.pem

.include <bsd.regress.mk>
