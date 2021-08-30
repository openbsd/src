#	$OpenBSD: Makefile,v 1.2 2021/08/30 17:28:46 tb Exp $

PROG=	clienttest
LDADD=	-lssl -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-g -DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
