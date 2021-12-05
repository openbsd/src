# $OpenBSD: Makefile,v 1.1 2021/12/05 13:01:08 jsing Exp $

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
	    ${.CURDIR}/../../libcrypto/ct/libressl.org.crt

.include <bsd.regress.mk>
