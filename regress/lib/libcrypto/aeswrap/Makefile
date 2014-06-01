#	$OpenBSD: Makefile,v 1.3 2014/06/01 11:11:45 miod Exp $

PROG=	aes_wrap
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-Werror

.include <bsd.regress.mk>
