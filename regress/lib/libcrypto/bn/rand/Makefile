#	$OpenBSD: Makefile,v 1.1 2018/11/06 06:55:27 tb Exp $

.include "../../Makefile.inc"

PROG=	bn_rand_interval
LDADD=	${CRYPTO_INT}
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-Werror

.include <bsd.regress.mk>
