#	$OpenBSD: Makefile,v 1.1 2014/05/06 20:27:19 miod Exp $

PROG=	pq_test
CRYPTO=	${.CURDIR}/../../../../lib/libssl/src/crypto
CFLAGS+= -I${CRYPTO}/pqueue

LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}

.include <bsd.regress.mk>
