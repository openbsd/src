# $OpenBSD: Makefile,v 1.5 2023/04/14 11:18:40 tb Exp $

PROG=		cttest
LDADD=		-lcrypto
DPADD=		${LIBCRYPTO}

WARNINGS=	Yes
CFLAGS+=	-Wundef -Werror
CFLAGS+=	-DLIBRESSL_CRYPTO_INTERNAL -DLIBRESSL_INTERNAL

run-regress-cttest: cttest
	./cttest ${.CURDIR}/../../libcrypto/ct/

.include <bsd.regress.mk>
