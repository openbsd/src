#	$OpenBSD: Makefile,v 1.12 2023/03/02 20:45:11 tb Exp $

PROGS +=	evp_ecx_test
PROGS +=	evp_pkey_check
PROGS +=	evp_pkey_cleanup
PROGS +=	evp_test
PROGS +=	evptest

LDADD =		-lcrypto
DPADD =		${LIBCRYPTO}
WARNINGS =	Yes
CFLAGS +=	-DLIBRESSL_INTERNAL -DLIBRESSL_CRYPTO_INTERNAL -Werror
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/curve25519
CFLAGS +=	-I${.CURDIR}/../../../../lib/libcrypto/evp

run-regress-evptest: evptest
	./evptest ${.CURDIR}/evptests.txt

run-regress-evptest-verbose: evptest
	./evptest -v ${.CURDIR}/evptests.txt

.PHONY: run-regress-evptest-verbose

.include <bsd.regress.mk>
