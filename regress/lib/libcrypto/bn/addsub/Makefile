#	$OpenBSD: Makefile,v 1.1 2018/07/10 16:57:50 tb Exp $

.include "../../Makefile.inc"

PROG=		bnaddsub
LDADD=		${CRYPTO_INT}
DPADD=		${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-Werror

.include <bsd.regress.mk>
