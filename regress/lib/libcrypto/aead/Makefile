#	$OpenBSD: Makefile,v 1.6 2022/07/30 16:30:42 jsing Exp $

PROG=	aeadtest
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

REGRESS_TARGETS=regress-aeadtest

regress-aeadtest: ${PROG}
	./${PROG} aead ${.CURDIR}/aeadtests.txt

.include <bsd.regress.mk>
