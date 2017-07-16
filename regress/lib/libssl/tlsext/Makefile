# $OpenBSD: Makefile,v 1.1 2017/07/16 18:18:10 jsing Exp $

PROG=	tlsexttest
LDADD=	${SSL_INT} -lcrypto
DPADD=	${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Wundef -Werror -I$(BSDSRCDIR)/lib/libssl

.include <bsd.regress.mk>
