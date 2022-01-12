#	$OpenBSD: Makefile,v 1.4 2022/01/12 08:54:23 tb Exp $

PROG=	aeadtest
#LDADD=	-lcrypto
LDADD=	${CRYPTO_INT} # XXX link dynamically after bump
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -DLIBRESSL_CRYPTO_INTERNAL -Werror

REGRESS_TARGETS=regress-aeadtest

regress-aeadtest: ${PROG}
	./${PROG} ${.CURDIR}/aeadtests.txt

.include <bsd.regress.mk>
