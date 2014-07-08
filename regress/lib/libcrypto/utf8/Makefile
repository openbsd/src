#	$OpenBSD: Makefile,v 1.4 2014/07/08 15:53:53 jsing Exp $

PROG=	utf8test
CPPFLAGS+=-I${.CURDIR}/../../../../lib/libssl/src/crypto/asn1
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
