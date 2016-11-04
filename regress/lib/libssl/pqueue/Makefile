#	$OpenBSD: Makefile,v 1.1 2016/11/04 19:45:12 jsing Exp $

PROG=	pq_test
SRC=	${.CURDIR}/../../../../lib/libssl
CFLAGS+=	-I${SRC}

LDADD=	${SSL_INT} -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

REGRESS_TARGETS= regress-pq_test

regress-pq_test: ${PROG}
	${.OBJDIR}/pq_test | cmp -s ${.CURDIR}/expected.txt /dev/stdin

.include <bsd.regress.mk>
