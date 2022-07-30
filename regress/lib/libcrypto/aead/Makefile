#	$OpenBSD: Makefile,v 1.7 2022/07/30 16:40:23 jsing Exp $

PROG=	aeadtest
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

REGRESS_TARGETS=regress-aeadtest

regress-aeadtest: ${PROG}
	./${PROG} aead ${.CURDIR}/aeadtests.txt
	./${PROG} aes-128-gcm ${.CURDIR}/aes_128_gcm_tests.txt
	./${PROG} aes-192-gcm ${.CURDIR}/aes_192_gcm_tests.txt
	./${PROG} aes-256-gcm ${.CURDIR}/aes_256_gcm_tests.txt
	./${PROG} chacha20-poly1305 ${.CURDIR}/chacha20_poly1305_tests.txt
	./${PROG} xchacha20-poly1305 ${.CURDIR}/xchacha20_poly1305_tests.txt

.include <bsd.regress.mk>
