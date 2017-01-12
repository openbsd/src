# $OpenBSD: Makefile,v 1.1 2017/01/12 15:50:16 jsing Exp $

PROG=	tlstest
LDADD=	-lcrypto -lssl -ltls
DPADD=	${LIBCRYPTO} ${LIBSSL} ${LIBTLS}

WARNINGS=	Yes
CFLAGS+=	-Werror

REGRESS_TARGETS= \
	regress-tlstest

regress-tlstest: ${PROG}
	./tlstest \
	    ${.CURDIR}/../../libssl/certs/server.pem \
	    ${.CURDIR}/../../libssl/certs/server.pem \
	    ${.CURDIR}/../../libssl/certs/ca.pem

.include <bsd.regress.mk>
