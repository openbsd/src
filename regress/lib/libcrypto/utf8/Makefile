#	$OpenBSD: Makefile,v 1.6 2016/12/21 15:51:05 jsing Exp $

PROG=	utf8test
CPPFLAGS+=-I${.CURDIR}/../../../../lib/libcrypto/asn1
LDADD=	${CRYPTO_INT}
DPADD=	${LIBCRYPTO}
WARNINGS=Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
