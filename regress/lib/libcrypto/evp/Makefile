#	$OpenBSD: Makefile,v 1.5 2022/01/12 09:11:48 tb Exp $

PROGS=	evptest evp_pkey_check
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -DLIBRESSL_CRYPTO_INTERNAL -Werror

REGRESS_TARGETS+=	regress-evptest
REGRESS_TARGETS+=	regress-evp_pkey_check

LDADD_evp_pkey_check= ${CRYPTO_INT}  # XXX remove after bump

regress-evptest:	evptest
	./evptest ${.CURDIR}/evptests.txt

regress-evp_pkey_check: evp_pkey_check
	./evp_pkey_check

.include <bsd.regress.mk>
