#	$OpenBSD: Makefile,v 1.5 2021/05/10 20:41:44 tb Exp $

PROG=	ecdhtest
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
