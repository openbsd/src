#	$OpenBSD: Makefile,v 1.3 2021/08/30 17:34:01 tb Exp $

PROG=	clienttest
LDADD=	-lssl -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
