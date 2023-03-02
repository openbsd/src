#	$OpenBSD: Makefile,v 1.10 2023/03/02 20:22:46 tb Exp $

PROGS=	evptest evp_test evp_ecx_test evp_pkey_check evp_pkey_cleanup
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -DLIBRESSL_CRYPTO_INTERNAL -Werror
CFLAGS+=	-I${.CURDIR}/../../../../lib/libcrypto/curve25519
CFLAGS+=	-I${.CURDIR}/../../../../lib/libcrypto/evp

LDADD_evp_ecx_test = ${CRYPTO_INT}

run-regress-evptest:	evptest
	./evptest ${.CURDIR}/evptests.txt

run-regress-evptest-verbose:
	./evptest -v ${.CURDIR}/evptests.txt

.PHONY: run-regress-evptest-verbose

.include <bsd.regress.mk>
