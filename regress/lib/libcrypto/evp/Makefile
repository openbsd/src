#	$OpenBSD: Makefile,v 1.7 2022/03/30 08:57:26 tb Exp $

PROGS=	evptest evp_pkey_check evp_pkey_cleanup
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -DLIBRESSL_CRYPTO_INTERNAL -Werror
CFLAGS+=	-I${.CURDIR}/../../../../lib/libcrypto/evp

REGRESS_TARGETS+=	regress-evptest
REGRESS_TARGETS+=	regress-evp_pkey_check
REGRESS_TARGETS+=	regress-evp_pkey_cleanup

regress-evptest:	evptest
	./evptest ${.CURDIR}/evptests.txt

regress-evp_pkey_check: evp_pkey_check
	./evp_pkey_check

regress-evp_pkey_cleanup: evp_pkey_cleanup
	./evp_pkey_cleanup

.include <bsd.regress.mk>
