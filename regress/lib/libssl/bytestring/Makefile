#	$OpenBSD: Makefile,v 1.3 2015/07/31 14:18:06 tobiasu Exp $

PROG=	bytestringtest
LDADD=	-lcrypto -lssl
DPADD=	${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Wundef -Werror -I$(BSDSRCDIR)/lib/libssl/src/ssl

.include <bsd.regress.mk>
