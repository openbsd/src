#	$OpenBSD: Makefile,v 1.3 2021/12/24 14:00:11 tb Exp $

.include "../../Makefile.inc"

PROG=	rfc3779
LDADD=	${CRYPTO_INT}
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_CRYPTO_INTERNAL -DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
