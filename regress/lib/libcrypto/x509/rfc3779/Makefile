#	$OpenBSD: Makefile,v 1.4 2022/01/06 14:55:52 tb Exp $

.include "../../Makefile.inc"

PROG=	rfc3779
LDADD=	${CRYPTO_INT}
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_CRYPTO_INTERNAL -DLIBRESSL_INTERNAL -Werror -g -O0

.include <bsd.regress.mk>
