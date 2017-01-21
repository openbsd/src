#	$OpenBSD: Makefile,v 1.4 2017/01/21 09:38:58 beck Exp $

PROG=	exptest
LDADD=	${CRYPTO_INT}
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-Werror

.include <bsd.regress.mk>
