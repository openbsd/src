#	$OpenBSD: Makefile,v 1.5 2015/09/14 01:45:03 doug Exp $

PROG=	md4test
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
