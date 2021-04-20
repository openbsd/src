#	$OpenBSD: Makefile,v 1.4 2021/04/20 17:09:45 tb Exp $

PROG=	ectest
LDADD=	${CRYPTO_INT}
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
