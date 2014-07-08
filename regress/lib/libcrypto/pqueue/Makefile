#	$OpenBSD: Makefile,v 1.6 2014/07/08 15:53:53 jsing Exp $

PROG=	pq_test
SRC=	${.CURDIR}/../../../../lib/libssl/src/ssl
CFLAGS+=	-I${SRC}

LDADD=	-lssl -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

REGRESS_TARGETS= regress-pq_test

regress-pq_test: ${PROG}
	${.OBJDIR}/pq_test | cmp -s ${.CURDIR}/expected.txt /dev/stdin

.include <bsd.regress.mk>
