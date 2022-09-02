#	$OpenBSD: Makefile,v 1.1.1.1 2022/09/02 13:34:48 tb Exp $

PROG=	md_test
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
