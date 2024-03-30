#	$OpenBSD: Makefile,v 1.4 2024/03/30 00:36:14 jsing Exp $

PROGS=		rsa_test \
		rsa_padding_test
LDADD=		-lcrypto
DPADD=		${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
