#	$OpenBSD: Makefile,v 1.2 2020/05/11 18:18:21 jsing Exp $

PROG=	servertest
LDADD=	${SSL_INT} -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

REGRESS_TARGETS= \
	regress-servertest

regress-servertest: ${PROG}
	./servertest \
	    ${.CURDIR}/../../libssl/certs/server.pem \
	    ${.CURDIR}/../../libssl/certs/server.pem \
	    ${.CURDIR}/../../libssl/certs/ca.pem

.include <bsd.regress.mk>
