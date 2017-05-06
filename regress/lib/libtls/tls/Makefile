# $OpenBSD: Makefile,v 1.2 2017/05/06 21:56:43 jsing Exp $

PROG=	tlstest
LDADD=	-lcrypto -lssl -ltls
DPADD=	${LIBCRYPTO} ${LIBSSL} ${LIBTLS}

WARNINGS=	Yes
CFLAGS+=	-Werror

REGRESS_TARGETS= \
	regress-tlstest

regress-tlstest: ${PROG}
	./tlstest \
	    ${.CURDIR}/../../libssl/certs/ca.pem \
	    ${.CURDIR}/../../libssl/certs/server.pem \
	    ${.CURDIR}/../../libssl/certs/server.pem

.include <bsd.regress.mk>
