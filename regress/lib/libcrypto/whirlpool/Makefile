#	$OpenBSD: Makefile,v 1.1 2024/03/29 07:13:38 joshua Exp $

PROG =          whirlpool_test
LDADD =         -lcrypto
DPADD =         ${LIBCRYPTO}
WARNINGS =      Yes
CFLAGS +=       -DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
