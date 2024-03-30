#	$OpenBSD: Makefile,v 1.2 2024/03/30 00:32:13 jsing Exp $

PROG=	rsa_test
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
