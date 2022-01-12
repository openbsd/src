#	$OpenBSD: Makefile,v 1.4 2022/01/12 09:02:34 tb Exp $

PROG=	ecdsatest
LDADD=	${CRYPTO_INT}	# XXX link dynamically after bump
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -DLIBRESSL_CRYPTO_INTERNAL -Werror

.include <bsd.regress.mk>
