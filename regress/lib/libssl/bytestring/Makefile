#	$OpenBSD: Makefile,v 1.4 2016/09/02 17:31:04 beck Exp $

PROG=	bytestringtest
LDADD=	-lcrypto -lssl
DPADD=	${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Wundef -Werror -I$(BSDSRCDIR)/lib/libssl

.include <bsd.regress.mk>
