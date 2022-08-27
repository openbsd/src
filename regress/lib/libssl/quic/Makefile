#	$OpenBSD: Makefile,v 1.1 2022/08/27 09:16:29 jsing Exp $

PROG=	quictest
LDADD=	${SSL_INT} -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}

WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

REGRESS_TARGETS= \
	regress-quictest

regress-quictest: ${PROG}
	./quictest \
	    ${.CURDIR}/../../libssl/certs/server.pem \
	    ${.CURDIR}/../../libssl/certs/server.pem \
	    ${.CURDIR}/../../libssl/certs/ca.pem

.include <bsd.regress.mk>
