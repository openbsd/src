#	$OpenBSD: Makefile,v 1.3 2014/06/01 11:11:46 miod Exp $

PROG=	gcm128test
SRC=	${.CURDIR}/../../../../lib/libssl/src/crypto/modes
CFLAGS+=-I${SRC}

LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
#WARNINGS=	Yes
#CFLAGS+=	-Werror

.include <bsd.regress.mk>
