#	$OpenBSD: Makefile,v 1.5 2014/06/01 11:11:46 miod Exp $

PROG=	pq_test
SRC=	${.CURDIR}/../../../../lib/libssl/src/ssl
CFLAGS+=	-I${SRC}

LDADD=	-lssl -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-Werror

REGRESS_TARGETS= regress-pq_test

regress-pq_test: ${PROG}
	${.OBJDIR}/pq_test | cmp -s ${.CURDIR}/expected.txt /dev/stdin

.include <bsd.regress.mk>
