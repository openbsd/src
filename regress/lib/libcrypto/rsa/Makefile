#	$OpenBSD: Makefile,v 1.5 2025/01/04 18:13:58 tb Exp $

PROGS +=	rsa_test
PROGS +=	rsa_padding_test

LDADD =		-lcrypto
DPADD =		${LIBCRYPTO}
WARNINGS =	Yes
CFLAGS +=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
