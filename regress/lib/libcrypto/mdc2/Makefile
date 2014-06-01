#	$OpenBSD: Makefile,v 1.2 2014/06/01 11:11:46 miod Exp $

PROG=	mdc2test
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-Werror

.include <bsd.regress.mk>
