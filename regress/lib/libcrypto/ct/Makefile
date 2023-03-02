# $OpenBSD: Makefile,v 1.4 2023/03/02 21:17:35 tb Exp $

PROG=		cttest
LDADD=		-lcrypto
DPADD=		${LIBCRYPTO}

WARNINGS=	Yes
CFLAGS+=	-Wundef -Werror
CFLAGS+=	-DLIBRESSL_CRYPTO_INTERNAL -DLIBRESSL_INTERNAL
CFLAGS+=	-I$(BSDSRCDIR)/lib/libcrypto

run-regress-cttest: cttest
	./cttest ${.CURDIR}/../../libcrypto/ct/

.include <bsd.regress.mk>
