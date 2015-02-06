#	$OpenBSD: Makefile,v 1.1 2015/02/06 08:48:39 jsing Exp $

PROG=	cipherstest
LDADD=	-lssl -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
