#	$OpenBSD: Makefile,v 1.7 2016/09/02 17:02:53 beck Exp $

PROG=	pq_test
SRC=	${.CURDIR}/../../../../lib/libssl
CFLAGS+=	-I${SRC}

LDADD=	-lssl -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

REGRESS_TARGETS= regress-pq_test

regress-pq_test: ${PROG}
	${.OBJDIR}/pq_test | cmp -s ${.CURDIR}/expected.txt /dev/stdin

.include <bsd.regress.mk>
