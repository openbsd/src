#	$OpenBSD: Makefile,v 1.2 2014/06/01 14:47:14 miod Exp $

PROG=	ssltest
LDADD=	-lcrypto -lssl
DPADD=	${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
CFLAGS+=	-Werror

REGRESS_TARGETS=regress-ssltest

regress-ssltest: ${PROG}
	sh ${.CURDIR}/testssl \
	  ${.CURDIR}/../certs/server.pem ${.CURDIR}/../certs/server.pem \
	  ${.CURDIR}/../certs/ca.pem

.include <bsd.regress.mk>
