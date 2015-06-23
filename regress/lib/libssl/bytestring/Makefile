#	$OpenBSD: Makefile,v 1.2 2015/06/23 00:02:01 doug Exp $

PROG=	bytestringtest
LDADD=	-lcrypto -lssl
DPADD=	${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Wundef -Werror -I/usr/src/lib/libssl/src/ssl

.include <bsd.regress.mk>
