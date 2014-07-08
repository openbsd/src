#	$OpenBSD: Makefile,v 1.4 2014/07/08 15:53:52 jsing Exp $

PROG=	gcm128test
SRC=	${.CURDIR}/../../../../lib/libssl/src/crypto/modes
CFLAGS+=-I${SRC}

LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
#WARNINGS=	Yes
#CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
