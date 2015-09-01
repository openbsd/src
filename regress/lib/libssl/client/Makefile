#	$OpenBSD: Makefile,v 1.1 2015/09/01 17:02:18 jsing Exp $

PROG=	clienttest
LDADD=	-lssl -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
