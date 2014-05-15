#	$OpenBSD: Makefile,v 1.4 2014/05/15 19:23:14 miod Exp $

PROG=	pq_test
SRC=	${.CURDIR}/../../../../lib/libssl/src/ssl
CFLAGS+=	-I${SRC}

LDADD=	-lssl -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}

REGRESS_TARGETS= regress-pq_test

regress-pq_test: ${PROG}
	${.OBJDIR}/pq_test | cmp -s ${.CURDIR}/expected.txt /dev/stdin

.include <bsd.regress.mk>
