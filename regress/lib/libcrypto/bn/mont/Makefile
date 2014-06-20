#	$OpenBSD: Makefile,v 1.1 2014/06/20 10:38:22 miod Exp $

PROG=	mont
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-Werror

.include <bsd.regress.mk>
