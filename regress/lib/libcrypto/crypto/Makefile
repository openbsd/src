#	$OpenBSD: Makefile,v 1.1 2024/04/25 14:27:29 jsing Exp $

PROG =	crypto_test

DPADD+=		${LIBCRYPTO}
WARNINGS=	Yes
LDFLAGS+=	-lcrypto
CFLAGS+=	-DLIBRESSL_INTERNAL
CFLAGS+=	-Wall -Wundef -Werror
CFLAGS+=	-I${.CURDIR}/../../../../lib/libcrypto

.include <bsd.regress.mk>
