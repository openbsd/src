#	$OpenBSD: Makefile,v 1.4 2021/10/15 16:49:12 jsing Exp $

PROG=	ssltest
LDADD=	-lcrypto -lssl
DPADD=	${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror
CFLAGS+=	-I${.CURDIR}/../../../../lib/libssl

REGRESS_TARGETS=regress-ssltest

regress-ssltest: ${PROG}
	sh ${.CURDIR}/testssl \
	  ${.CURDIR}/../certs/server.pem ${.CURDIR}/../certs/server.pem \
	  ${.CURDIR}/../certs/ca.pem

.include <bsd.regress.mk>
