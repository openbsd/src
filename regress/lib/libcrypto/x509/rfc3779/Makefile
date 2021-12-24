#	$OpenBSD: Makefile,v 1.1 2021/12/24 03:00:37 tb Exp $

.include "../../Makefile.inc"

PROG=	rfc3779
LDADD=	${CRYPTO_INT}
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror -g -O0

.include <bsd.regress.mk>
