#	$OpenBSD: Makefile,v 1.3 2014/07/08 15:53:53 jsing Exp $

PROG=	ssltest
LDADD=	-lcrypto -lssl
DPADD=	${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

REGRESS_TARGETS=regress-ssltest

regress-ssltest: ${PROG}
	sh ${.CURDIR}/testssl \
	  ${.CURDIR}/../certs/server.pem ${.CURDIR}/../certs/server.pem \
	  ${.CURDIR}/../certs/ca.pem

.include <bsd.regress.mk>
