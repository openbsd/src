#	$OpenBSD: Makefile,v 1.2 2014/05/31 14:39:06 jsing Exp $

PROG=	gcm128test
SRC=	${.CURDIR}/../../../../lib/libssl/src/crypto/modes
CFLAGS+=-I${SRC}

LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}

.include <bsd.regress.mk>
