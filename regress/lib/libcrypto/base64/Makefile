#	$OpenBSD: Makefile,v 1.2 2014/06/01 11:11:45 miod Exp $

PROG=	base64test
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-Werror

.include <bsd.regress.mk>
