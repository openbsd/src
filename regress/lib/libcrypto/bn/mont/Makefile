#	$OpenBSD: Makefile,v 1.3 2017/01/21 09:38:58 beck Exp $

PROG=	mont
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-Werror

.include <bsd.regress.mk>
