#	$OpenBSD: Makefile,v 1.4 2022/01/12 08:59:56 tb Exp $

PROG=	dsatest
LDADD=	${CRYPTO_INT}	# XXX link dynamically after bump
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -DLIBRESSL_CRYPTO_INTERNAL -Werror

REGRESS_TARGETS=regress-dsatest

regress-dsatest:	${PROG}
	./${PROG}
	./${PROG} -app2_1

.include <bsd.regress.mk>
