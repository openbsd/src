#	$OpenBSD: Makefile,v 1.4 2022/01/11 19:20:36 tb Exp $

PROGS=	evptest evp_pkey_check
LDADD=	${CRYPTO_INT}
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -DLIBRESSL_CRYPTO_INTERNAL -Werror

REGRESS_TARGETS+=	regress-evptest
REGRESS_TARGETS+=	regress-evp_pkey_check

regress-evptest:	evptest
	./evptest ${.CURDIR}/evptests.txt

regress-evp_pkey_check: evp_pkey_check
	./evp_pkey_check

.include <bsd.regress.mk>
