#	$OpenBSD: Makefile,v 1.1 2015/02/06 09:36:16 doug Exp $

PROG=	bytestringtest
LDADD=	-lcrypto -lssl
DPADD=	${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror -I/usr/src/lib/libssl/src/ssl

REGRESS_TARGETS=regress-bytestringtest

regress-bytestringtest: ${PROG}
	./${PROG}

.include <bsd.regress.mk>
