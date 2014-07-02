#	$OpenBSD: Makefile,v 1.1 2014/07/02 16:29:36 jsing Exp $

PROG=	pkcs7test
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-Werror

.include <bsd.regress.mk>
