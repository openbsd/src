#	$OpenBSD: Makefile,v 1.2 2014/05/22 03:36:23 guenther Exp $

PROG=	utf8test
CPPFLAGS+=-I${.CURDIR}/../../../../lib/libssl/src/crypto/asn1
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=Yes

.include <bsd.regress.mk>
