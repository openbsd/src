#	$OpenBSD: Makefile,v 1.1 2014/06/20 10:30:40 miod Exp $

PROG=	bntest
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-Werror

.include <bsd.regress.mk>
