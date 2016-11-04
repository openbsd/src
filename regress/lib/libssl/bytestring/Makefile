#	$OpenBSD: Makefile,v 1.5 2016/11/04 17:51:54 guenther Exp $

PROG=	bytestringtest
LDADD=	${SSL_INT} -lcrypto
DPADD=	${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Wundef -Werror -I$(BSDSRCDIR)/lib/libssl

.include <bsd.regress.mk>
