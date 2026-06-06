#	$OpenBSD: Makefile,v 1.6 2026/06/06 08:13:50 jsing Exp $

PROGS +=	dtlstest
PROGS +=	dtls_handshake_msg_test
PROGS +=	dtls_wire_test

LDADD=		${SSL_INT} -lcrypto
DPADD=		${LIBSSL} ${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror
CFLAGS+=	-I${.CURDIR}/../../../../lib/libcrypto/bio
CFLAGS+=	-I${.CURDIR}/../../../../lib/libssl

REGRESS_TARGETS= \
	regress-dtlstest \
	regress-dtls_handshake_msg_test \
	regress-dtls_wire_test

# XXX(jsing): use CA root and chain
regress-dtlstest: dtlstest
	./dtlstest \
	    ${.CURDIR}/../../libssl/certs/server1-rsa.pem \
	    ${.CURDIR}/../../libssl/certs/server1-rsa.pem \
	    ${.CURDIR}/../../libssl/certs/ca-int-rsa.pem

regress-dtls_handshake_msg_test: dtls_handshake_msg_test
	./dtls_handshake_msg_test

regress-dtls_wire_test: dtls_wire_test
	./dtls_wire_test

.include <bsd.regress.mk>
