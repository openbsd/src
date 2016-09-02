#	$OpenBSD: Makefile,v 1.5 2016/09/02 17:02:56 beck Exp $

PROG=	utf8test
CPPFLAGS+=-I${.CURDIR}/../../../../lib/libcrypto/asn1
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
