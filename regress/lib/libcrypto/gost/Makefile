#	$OpenBSD: Makefile,v 1.1 2014/11/09 19:45:26 miod Exp $

PROG=	gost2814789t
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
