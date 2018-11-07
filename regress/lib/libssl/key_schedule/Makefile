# $OpenBSD: Makefile,v 1.1 2018/11/07 19:43:12 beck Exp $

PROG=	key_schedule
LDADD=	${SSL_INT} -lcrypto
DPADD=	${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Wundef -Werror -I$(BSDSRCDIR)/lib/libssl

.include <bsd.regress.mk>
