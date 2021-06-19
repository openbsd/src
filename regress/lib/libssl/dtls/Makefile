#	$OpenBSD: Makefile,v 1.2 2021/06/19 17:11:34 jsing Exp $

PROG=		dtlstest
LDADD=		${SSL_INT} -lcrypto
DPADD=		${LIBSSL} ${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror
CFLAGS+=	-I${.CURDIR}/../../../../lib/libssl

REGRESS_TARGETS= \
	regress-dtlstest

regress-dtlstest: ${PROG}
	./dtlstest \
	    ${.CURDIR}/../../libssl/certs/server.pem \
	    ${.CURDIR}/../../libssl/certs/server.pem \
	    ${.CURDIR}/../../libssl/certs/ca.pem

.include <bsd.regress.mk>
