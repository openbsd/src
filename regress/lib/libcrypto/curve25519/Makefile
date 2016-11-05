#	$OpenBSD: Makefile,v 1.1 2016/11/05 15:09:02 jsing Exp $

PROG=	x25519test
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
