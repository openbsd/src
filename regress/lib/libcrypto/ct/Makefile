# $OpenBSD: Makefile,v 1.2 2022/01/06 04:42:00 jsing Exp $

PROG=		cttest
LDADD=		${CRYPTO_INT}
DPADD=		${LIBCRYPTO}

WARNINGS=	Yes
CFLAGS+=	-Wundef -Werror
CFLAGS+=	-DLIBRESSL_CRYPTO_INTERNAL -DLIBRESSL_INTERNAL
CFLAGS+=	-I$(BSDSRCDIR)/lib/libcrypto

REGRESS_TARGETS= \
	regress-cttest

regress-cttest: ${PROG}
	./cttest \
	    ${.CURDIR}/../../libcrypto/ct/

.include <bsd.regress.mk>
