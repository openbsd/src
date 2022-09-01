#	$OpenBSD: Makefile,v 1.5 2022/09/01 14:02:41 tb Exp $

PROG =		sha_test
LDADD =		-lcrypto
DPADD =		${LIBCRYPTO}
WARNINGS =	Yes
CFLAGS +=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
